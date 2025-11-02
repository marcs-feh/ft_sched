import time
import random
import struct
from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from pyocd.flash.file_programmer import FileProgrammer
from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection
import csv
from datetime import datetime

class STM32FaultInjector:
    def __init__(self, target_type='STM32F411CEUx', elf_path=None):
        """
        Initialize fault injector for STM32 using PyOCD and ST-Link
        
        Args:
            target_type: Target MCU type (default: stm32f411ceu6)
            elf_path: Path to .elf file with debug symbols
        """
        self.target_type = target_type
        self.elf_path = elf_path
        self.session = None
        self.target = None
        self.symbols = {}
        
        if elf_path:
            self.load_symbols(elf_path)
    
    def load_symbols(self, elf_path):
        """
        Parse ELF file and extract symbol table
        
        Args:
            elf_path: Path to the .elf file
        """
        print(f"Loading symbols from {elf_path}...")
        
        try:
            with open(elf_path, 'rb') as f:
                elf = ELFFile(f)
                
                # Iterate through all sections to find symbol tables
                for section in elf.iter_sections():
                    if isinstance(section, SymbolTableSection):
                        for symbol in section.iter_symbols():
                            if symbol.name and symbol['st_value'] != 0:
                                self.symbols[symbol.name] = {
                                    'address': symbol['st_value'],
                                    'size': symbol['st_size'],
                                    'type': symbol['st_info']['type']
                                }
                
                print(f"Loaded {len(self.symbols)} symbols")
                return True
                
        except Exception as e:
            print(f"Failed to load symbols: {e}")
            return False
    
    def find_symbol(self, symbol_name):
        """
        Find a symbol by name
        
        Args:
            symbol_name: Name of the symbol to find
            
        Returns:
            Dictionary with address, size, and type, or None if not found
        """
        if symbol_name in self.symbols:
            sym = self.symbols[symbol_name]
            print(f"Symbol '{symbol_name}' found:")
            print(f"  Address: 0x{sym['address']:08X}")
            print(f"  Size: {sym['size']} bytes")
            print(f"  Type: {sym['type']}")
            return sym
        else:
            print(f"Symbol '{symbol_name}' not found")
            # Search for partial matches
            matches = [s for s in self.symbols.keys() if symbol_name.lower() in s.lower()]
            if matches:
                print(f"Did you mean one of these?")
                for match in matches[:5]:
                    print(f"  - {match}")
            return None
    
    def list_symbols(self, pattern=None):
        """
        List all symbols, optionally filtered by pattern
        
        Args:
            pattern: Optional string to filter symbol names
        """
        matching = self.symbols.keys()
        if pattern:
            matching = [s for s in matching if pattern.lower() in s.lower()]
        
        print(f"\nFound {len(matching)} symbols" + (f" matching '{pattern}'" if pattern else ""))
        for symbol_name in sorted(matching)[:20]:  # Show first 20
            sym = self.symbols[symbol_name]
            print(f"  {symbol_name:40s} @ 0x{sym['address']:08X} ({sym['size']} bytes)")
        
        if len(matching) > 20:
            print(f"  ... and {len(matching) - 20} more")
    
    def connect(self):
        """Establish connection to the target MCU via ST-Link"""
        try:
            # Connect to the target
            self.session = ConnectHelper.session_with_chosen_probe(
                target_override=self.target_type,
                options={
                    'frequency': 4000000,  # 4 MHz SWD frequency
                    'connect_mode': 'under-reset'
                }
            )
            
            self.session.open()
            self.target = self.session.target
            
            print(f"Connected to ST-Link")
            print(f"Target: {self.target.part_number}")
            # print(f"Core: {self.target.core.core_type}")
            # print(f"State: {self.target.get_state()}")
            
            # Halt the target initially
            if self.target.get_state() == Target.State.RUNNING:
                self.target.halt()
                print("Target halted")
            
            return True
            
        except Exception as e:
            print(f"Connection failed: {e}")
            print("\nTroubleshooting tips:")
            print("  - Make sure ST-Link drivers are installed")
            print("  - Check that ST-Link is connected via USB")
            print("  - Verify target MCU has power")
            print("  - Try: pyocd list  (to see available probes)")
            return False
    
    def read_memory_u32(self, address, count=1):
        """
        Read 32-bit words from memory
        
        Args:
            address: Starting address
            count: Number of 32-bit words to read
            
        Returns:
            List of values
        """
        try:
            values = self.target.read_memory_block32(address, count)
            return values
        except Exception as e:
            print(f"Memory read failed: {e}")
            return None
    
    def write_memory_u32(self, address, values):
        """
        Write 32-bit words to memory
        
        Args:
            address: Starting address
            values: List of 32-bit values to write
        """
        try:
            if not isinstance(values, list):
                values = [values]
            self.target.write_memory_block32(address, values)
            return True
        except Exception as e:
            print(f"Memory write failed: {e}")
            return False
    
    def read_symbol_value(self, symbol_name):
        """
        Read the value at a symbol's address
        
        Args:
            symbol_name: Name of the symbol
            
        Returns:
            List of values read from memory
        """
        sym = self.find_symbol(symbol_name)
        if not sym:
            return None
        
        address = sym['address']
        size = sym['size']
        
        # Determine how many 32-bit words to read
        num_words = (size + 3) // 4
        
        data = self.read_memory_u32(address, num_words)
        
        if data:
            print(f"\nValue at {symbol_name} (0x{address:08X}):")
            for i, value in enumerate(data):
                offset_addr = address + (i * 4)
                print(f"  +{i*4:04d} (0x{offset_addr:08X}): 0x{value:08X}")
        
        return data
    
    def inject_memory_bitflip(self, address, bit_position):
        """
        Inject a single bit flip at a specific memory address
        
        Args:
            address: Memory address to corrupt
            bit_position: Bit to flip (0-31 for 32-bit word)
            
        Returns:
            Tuple of (original_value, corrupted_value)
        """
        try:
            # Read current value
            original_value = self.read_memory_u32(address, 1)[0]
            print(f"Original value at 0x{address:08X}: 0x{original_value:08X}")
            
            # Flip the specified bit
            corrupted_value = original_value ^ (1 << bit_position)
            print(f"Corrupted value: 0x{corrupted_value:08X} (bit {bit_position} flipped)")
            
            # Write corrupted value back
            self.write_memory_u32(address, [corrupted_value])
            
            # Verify the injection
            verify_value = self.read_memory_u32(address, 1)[0]
            print(f"Verified value: 0x{verify_value:08X}")
            
            return original_value, corrupted_value
            
        except Exception as e:
            print(f"Bit flip injection failed: {e}")
            return None, None
    
    def inject_bitflip_in_symbol(self, symbol_name, offset=0, bit_position=None):
        """
        Inject a bit flip into a specific symbol
        
        Args:
            symbol_name: Name of the symbol
            offset: Byte offset within the symbol (default: 0)
            bit_position: Specific bit to flip, or None for random
            
        Returns:
            Tuple of (original_value, corrupted_value)
        """
        sym = self.find_symbol(symbol_name)
        if not sym:
            return None, None
        
        target_address = sym['address'] + offset
        
        if bit_position is None:
            bit_position = random.randint(0, 31)
        
        print(f"\nInjecting fault in '{symbol_name}' + {offset} bytes")
        return self.inject_memory_bitflip(target_address, bit_position)
    
    def dump_arena(self, arena_symbol):
        """
        Dump arena structure with proper field names
        
        Arena layout (20 bytes total):
            void* data           (4 bytes, offset 0)
            usize offset         (4 bytes, offset 4)
            usize capacity       (4 bytes, offset 8)
            void* last_allocation (4 bytes, offset 12)
            i32 region_count     (4 bytes, offset 16)
        
        Args:
            arena_symbol: Symbol name of the arena
            
        Returns:
            Dictionary with arena fields
        """
        sym = self.find_symbol(arena_symbol)
        if not sym:
            return None
        
        try:
            # Read 5 words
            data = self.read_memory_u32(sym['address'], 5)
            
            if not data:
                return None
            
            arena_data = {
                'data': data[0],
                'offset': data[1],
                'capacity': data[2],
                'last_allocation': data[3],
                'region_count': struct.unpack('i', data[4].to_bytes(4, 'little'))[0]  # signed int
            }
            
            print(f"\nArena '{arena_symbol}' at 0x{sym['address']:08X}:")
            print(f"  data:            0x{arena_data['data']:08X}")
            
            if arena_data['capacity'] > 0:
                usage_pct = (arena_data['offset'] * 100) // arena_data['capacity']
            else:
                usage_pct = 0
                
            print(f"  offset:          {arena_data['offset']} / {arena_data['capacity']} bytes ({usage_pct}% used)")
            print(f"  capacity:        {arena_data['capacity']} bytes")
            print(f"  last_allocation: 0x{arena_data['last_allocation']:08X}")
            print(f"  region_count:    {arena_data['region_count']}")
            
            # Validate arena state
            print(f"\n  Status checks:")
            print(f"    Data pointer valid (SRAM): {0x20000000 <= arena_data['data'] <= 0x20020000}")
            print(f"    Offset <= capacity: {arena_data['offset'] <= arena_data['capacity']}")
            print(f"    Capacity reasonable: {0 < arena_data['capacity'] <= 128*1024}")
            
            return arena_data
            
        except Exception as e:
            print(f"Failed to dump arena: {e}")
            return None
    
    def inject_arena_field_fault(self, arena_symbol, field, bit_position=None):
        """
        Inject a fault into a specific arena field
        
        Args:
            arena_symbol: Symbol name of the arena
            field: Field name ('data', 'offset', 'capacity', 'last_allocation', 'region_count')
            bit_position: Specific bit to flip, or None for random
            
        Returns:
            Tuple of (original_value, corrupted_value)
        """
        field_offsets = {
            'data': 0,
            'offset': 4,
            'capacity': 8,
            'last_allocation': 12,
            'region_count': 16
        }
        
        if field not in field_offsets:
            print(f"Unknown field '{field}'. Valid fields: {list(field_offsets.keys())}")
            return None, None
        
        offset = field_offsets[field]
        print(f"\nTargeting arena field: {field} (offset +{offset} bytes)")
        
        return self.inject_bitflip_in_symbol(arena_symbol, offset=offset, bit_position=bit_position)
    
    def arena_fault_campaign(self, arena_symbol, fault_scenarios, log_file=None):
        """
        Run targeted fault injection campaign on arena
        
        Args:
            arena_symbol: Symbol name of the arena
            fault_scenarios: List of tuples (field_name, bit_position, description)
            log_file: Optional CSV file to log results
            
        Returns:
            List of result dictionaries
        """
        print(f"\n{'='*70}")
        print(f"ARENA FAULT INJECTION CAMPAIGN: {arena_symbol}")
        print(f"{'='*70}")
        
        # Read initial state
        print("\n--- Initial Arena State ---")
        initial_state = self.dump_arena(arena_symbol)
        
        results = []
        
        # Create CSV logger if specified
        csv_writer = None
        csv_file = None
        if log_file:
            csv_file = open(log_file, 'w', newline='')
            csv_writer = csv.writer(csv_file)
            csv_writer.writerow([
                'timestamp', 'scenario', 'field', 'bit_position',
                'original_value', 'corrupted_value',
                'data_ptr', 'offset', 'capacity', 'last_alloc', 'region_count',
                'offset_valid', 'ptr_valid', 'capacity_valid'
            ])
        
        for i, (field, bit_pos, description) in enumerate(fault_scenarios):
            print(f"\n{'='*70}")
            print(f"Scenario {i+1}/{len(fault_scenarios)}: {description}")
            print(f"{'='*70}")
            
            # Inject fault
            orig, corr = self.inject_arena_field_fault(arena_symbol, field, bit_pos)
            
            # Read new state
            print(f"\n--- Arena State After Fault ---")
            new_state = self.dump_arena(arena_symbol)
            
            result = {
                'timestamp': datetime.now().isoformat(),
                'scenario': description,
                'field': field,
                'bit': bit_pos,
                'original': orig,
                'corrupted': corr,
                'after': new_state
            }
            
            results.append(result)
            
            # Log to CSV
            if csv_writer and new_state:
                csv_writer.writerow([
                    result['timestamp'],
                    description,
                    field,
                    bit_pos,
                    f"0x{orig:08X}" if orig else 'N/A',
                    f"0x{corr:08X}" if corr else 'N/A',
                    f"0x{new_state['data']:08X}",
                    new_state['offset'],
                    new_state['capacity'],
                    f"0x{new_state['last_allocation']:08X}",
                    new_state['region_count'],
                    new_state['offset'] <= new_state['capacity'],
                    0x20000000 <= new_state['data'] <= 0x20020000,
                    0 < new_state['capacity'] <= 128*1024
                ])
            
            # Wait between faults
            time.sleep(0.5)
        
        if csv_file:
            csv_file.close()
            print(f"\nResults logged to {log_file}")
        
        return results
    
    def resume(self):
        """Resume target execution"""
        try:
            self.target.resume()
            print("Target resumed")
        except Exception as e:
            print(f"Resume failed: {e}")
    
    def halt(self):
        """Halt target execution"""
        try:
            self.target.halt()
            print("Target halted")
        except Exception as e:
            print(f"Halt failed: {e}")
    
    def reset(self, halt=True):
        """Reset the target"""
        try:
            self.target.reset_and_halt() if halt else self.target.reset()
            print(f"Target reset ({'halted' if halt else 'running'})")
        except Exception as e:
            print(f"Reset failed: {e}")
    
    def disconnect(self):
        """Close the connection"""
        if self.session:
            self.session.close()
            print("\nDisconnected from target")


# Example usage
if __name__ == "__main__":
    # Create fault injector with ELF file
    injector = STM32FaultInjector(
        target_type='stm32f411ceux',
        elf_path='stm32.elf'
    )
    
    # Connect to the target
    if injector.connect():
        print("\n" + "="*60)
        
        # Example 1: Find arena symbols
        print("\nExample 1: Finding Arena Symbols")
        injector.list_symbols('arena')
        
        # Example 2: Dump arena structure
        print("\n" + "="*60)
        print("\nExample 2: Dump Arena State")
        injector.dump_arena('main_arena')
        
        # Example 3: Inject fault into specific arena field
        print("\n" + "="*60)
        print("\nExample 3: Corrupt Arena Offset")
        injector.inject_arena_field_fault('main_arena', 'offset', bit_position=16)
        
        # Example 4: Run comprehensive fault campaign with logging
        print("\n" + "="*60)
        print("\nExample 4: Arena Fault Campaign")
        
        fault_scenarios = [
            ('data', 8, 'Corrupt data pointer - low byte'),
            ('offset', 31, 'Corrupt offset - sign bit flip'),
            ('capacity', 0, 'Corrupt capacity - LSB flip'),
            ('last_allocation', 16, 'Corrupt last_allocation pointer'),
            ('offset', 16, 'Corrupt offset - middle bits'),
        ]
        
        results = injector.arena_fault_campaign(
            'main_arena',
            fault_scenarios,
            log_file='fault_injection_results.csv'
        )
        
        # Example 5: Reset and resume
        print("\n" + "="*60)
        print("\nExample 5: Reset Target")
        injector.reset(halt=True)
        
        # Disconnect
        injector.disconnect()
    else:
        print("Failed to connect to target")
        print("\nTo see available targets, run: pyocd list")
