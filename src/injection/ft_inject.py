import time
import random
import struct
import csv
import os

from datetime import datetime
from contextlib import contextmanager
from dataclasses import dataclass
from typing import Any, Generator

from pyocd.core.helpers import ConnectHelper
from pyocd.core.target import Target
from pyocd.flash.file_programmer import FileProgrammer

from elftools.elf.elffile import ELFFile
from elftools.elf.sections import SymbolTableSection

@dataclass
class Symbol:
    name : str
    address : int
    size : int
    type : Any

ELF_TYPENAME_CONV = {
    'STT_NOTYPE': None,
    'STT_OBJECT': 'Object',
    'STT_FUNC': 'Func',
    'STT_SECTION': 'Section',
    'STT_FILE': 'File',
    'STT_COMMON': 'Common',
}

class STM32Connection:
    """Active connection to STM32 target via ST-Link"""
    
    def __init__(self, session, target, symbols, line_info) -> None:
        self.session = session
        self.target = target
        self.symbols = symbols
        self.line_info = line_info
        self.breakpoints = {}
    
    def __enter__(self):
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        if self.session:
            self.session.close()
            print("[Disconnect]")
        return False
    
    def get_symbol(self, symbol_name) -> Symbol | None:
        """
        Get a symbol by name
        """
        try:
            sym = self.symbols[symbol_name]
            return sym
        except KeyError:
            return None
    
    def list_symbols(self, pattern: str = '') -> list[Symbol]:
        """
        List all symbols, optionally filtered by pattern
        
        Args:
            pattern: Symbol name substring
        """
        matching = self.symbols.keys()
        if pattern:
            matching = [s for s in matching if pattern.lower() in s.lower()]
        
        result = []

        for symbol_name in sorted(matching):
            result.append(self.symbols[symbol_name])
        return result

    def set_breakpoint_at_line(self, source_file: str, line_number: int) -> int | None:
        """
        Set a hardware breakpoint at a specific source file and line
            
        Returns:
            Breakpoint address if successful, None otherwise
        """

        source_file_normalized = os.path.normpath(source_file).replace('\\', '/')
        
        matching_addresses = []
        for (file_path, line), address in self.line_info.items():
            file_normalized = os.path.normpath(file_path).replace('\\', '/')

            if (file_normalized.endswith(source_file_normalized) or 
                source_file_normalized in file_normalized):
                if line == line_number:
                    matching_addresses.append((file_path, address))
        
        if not matching_addresses:
            print(f"  No address found for {source_file}:{line_number}")
            print(f"  Searching for similar files...")
            
            # Try to find similar file names
            similar_files = set()
            for (file_path, _), _ in self.line_info.items():
                if source_file.lower() in file_path.lower():
                    similar_files.add(file_path)
            
            if similar_files:
                print("  Did you mean one of these files?")
                for f in list(similar_files)[:5]:
                    print(f"    - {f}")
            
            return None
        
        # Use the first matching address
        file_path, address = matching_addresses[0]
        
        if len(matching_addresses) > 1:
            print(f"  Warning: Multiple addresses found for {source_file}:{line_number}")
            print(f"  Using first match: 0x{address:08X}")
        
        try:
            # Set hardware breakpoint
            bp = self.target.set_breakpoint(address)
            bp_id = id(bp)  # Use object id as breakpoint identifier
            self.breakpoints[bp_id] = address
            
            print(f"Breakpoint set at {file_path}:{line_number}")
            print(f"  Address: 0x{address:08X}")
            print(f"  Breakpoint ID: {bp_id}")
            
            return address
            
        except Exception as e:
            print(f"  Failed to set breakpoint: {e}")
            return None

    def remove_breakpoint(self, bp_id) -> None:
        """
        Remove a breakpoint by ID
        
        Args:
            bp_id: Breakpoint ID returned by set_breakpoint_at_line
        """
        try:
            if bp_id in self.breakpoints:
                address = self.breakpoints[bp_id]
                self.target.remove_breakpoint(address)
                del self.breakpoints[bp_id]
                print(f"Removed breakpoint at 0x{address:08X}")
            else:
                print(f"Breakpoint ID {bp_id} not found")
        except Exception as e:
            print(f"Failed to remove breakpoint: {e}")
    
    def clear_all_breakpoints(self) -> None:
        """Remove all breakpoints"""
        try:
            for bp_id in list(self.breakpoints.keys()):
                self.remove_breakpoint(bp_id)
            print("All breakpoints cleared")
        except Exception as e:
            print(f"Failed to clear breakpoints: {e}")
    
    def wait_for_breakpoint(self, timeout_ms: int=20000) -> bool:
        """
        Wait for target to hit a breakpoint
        
        Args:
            timeout_ms: Timeout in milliseconds
            
        Returns:
            True if breakpoint hit, False if timeout
        """
        try:
            # Resume execution
            self.target.resume()
            
            # Wait for halt with timeout
            start_time = time.time()
            while True:
                if self.target.get_state() == Target.State.HALTED:
                    pc = self.target.read_core_register('pc')
                    print(f"Breakpoint hit! PC = 0x{pc:08X}")
                    return True
                
                elapsed_ms = (time.time() - start_time) * 1000
                if elapsed_ms > timeout_ms:
                    print(f"Timeout waiting for breakpoint ({timeout_ms}ms)")
                    self.target.halt()
                    return False
                
                time.sleep(0.01)  # Poll every 10ms
                
        except Exception as e:
            print(f"Error waiting for breakpoint: {e}")
            return False
    def run_until_line(self, source_file: str, line_number: int, timeout_ms: int=20_000) -> bool:
        """
        Set a temporary breakpoint and run until it's hit
        
        Args:
            source_file: Source file name
            line_number: Line number
            timeout_ms: Timeout in milliseconds
            
        Returns:
            True if reached the line, False otherwise
        """
        address = self.set_breakpoint_at_line(source_file, line_number)
        if not address:
            return False
        
        result = self.wait_for_breakpoint(timeout_ms)
        
        # Note: Breakpoint remains set, call clear_all_breakpoints() to remove
        return result

    
    def read_memory_u32(self, address: int, count: int = 1):
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
    
    def write_memory_u32(self, address, values) -> bool:
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
        sym = self.get_symbol(symbol_name)
        if sym is None:
            return None
        
        address = sym.address
        size = sym.size
        
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
            original_value = self.read_memory_u32(address, 1)
            if original_value is not None:
                original_value = original_value[0]
            print(f"Original value at 0x{address:08X}: 0x{original_value:08X}")
            
            # Flip the specified bit
            corrupted_value = original_value ^ (1 << bit_position)
            print(f"Corrupted value: 0x{corrupted_value:08X} (bit {bit_position} flipped)")
            
            # Write corrupted value back
            self.write_memory_u32(address, [corrupted_value])
            
            # Verify the injection
            verify_value = self.read_memory_u32(address, 1)
            if verify_value is not None:
                verify_value = verify_value[0]
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
        sym = self.get_symbol(symbol_name)
        if not sym:
            return None, None
        
        target_address = sym.address + offset
        
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
        sym = self.get_symbol(arena_symbol)
        if not sym:
            return None
        
        try:
            # Read 5 words
            data = self.read_memory_u32(sym.address, 5)
            
            if not data:
                return None
            
            arena_data = {
                'data': data[0],
                'offset': data[1],
                'capacity': data[2],
                'last_allocation': data[3],
                'region_count': struct.unpack('i', data[4].to_bytes(4, 'little'))[0]  # signed int
            }
            
            print(f"\nArena '{arena_symbol}' at 0x{sym.address:08X}:")
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
        
        try:
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
        
        finally:
            if csv_file:
                csv_file.close()
                print(f"\nResults logged to {log_file}")
        
        return results
    
    def resume(self) -> None:
        """Resume target execution"""
        try:
            self.target.resume()
            print("Target resumed")
        except Exception as e:
            print(f"Resume failed: {e}")
    
    def halt(self) -> None:
        """Halt target execution"""
        try:
            self.target.halt()
            print("Target halted")
        except Exception as e:
            print(f"Halt failed: {e}")
    
    def reset(self, halt: bool=True) -> None:
        """Reset the target"""
        try:
            self.target.reset_and_halt() if halt else self.target.reset()
            print(f"Target reset ({'halted' if halt else 'running'})")
        except Exception as e:
            print(f"Reset failed: {e}")


class STM32FaultInjector:
    """Fault injector for STM32 microcontrollers using PyOCD and ST-Link"""
    
    def __init__(self, target_type: str, elf_path: str) -> None:
        """
        Initialize fault injector for STM32
        
        Args:
            target_type: Target MCU type (default: stm32f411ceu6)
            elf_path: Path to .elf file with debug symbols
        """
        self.target_type = target_type
        self.elf_path = elf_path
        self.symbols = {}
        self.line_info = {}

        if elf_path:
            self.load_symbols(elf_path)
            self.load_line_info(elf_path)
    
    def load_symbols(self, elf_path: str) -> bool:
        """
        Parse ELF file and extract symbol table
        
        Args:
            elf_path: Path to the .elf file
            
        Returns:
            True if successful, False otherwise
        """
        print(f"[Symbol Loading]")
        
        try:
            print(f"  Open {elf_path}...")
            with open(elf_path, 'rb') as f:
                elf = ELFFile(f)
                
                # Iterate through all sections to find symbol tables
                for section in elf.iter_sections():
                    if isinstance(section, SymbolTableSection):
                        for symbol in section.iter_symbols():
                            if symbol.name and symbol['st_value'] != 0:
                                sym_type = symbol['st_info']['type']
                                try:
                                    sym_type = ELF_TYPENAME_CONV[sym_type]
                                except KeyError:
                                    sym_type = 'Unknown'

                                self.symbols[symbol.name] = Symbol(
                                    address = symbol['st_value'],
                                    size = symbol['st_size'],
                                    type = sym_type,
                                    name = symbol.name,
                                )
                
                print(f"  Loaded {len(self.symbols)} symbols")
                return True
                
        except Exception as e:
            print(f"  Failed to load symbols: {e}")
            return False
    
    @contextmanager
    def connect(self) -> Generator[STM32Connection]:
        """
        Establish connection to the target MCU via ST-Link
        
        Yields:
            STM32Connection: Active connection object
        """
        session = None
        print('[Connect to MCU]')
        try:
            # Connect to the target
            session = ConnectHelper.session_with_chosen_probe(
                target_override=self.target_type,
                options={
                    'frequency': 4000000,  # 4 MHz SWD frequency
                    'connect_mode': 'under-reset'
                }
            )
            
            assert session is not None
            session.open()
            target = session.target
            assert target is not None
            
            print(f"  Connected to {target.part_number} | {target.get_state()}")
            
            # Halt the target initially
            if target.get_state() == Target.State.RUNNING:
                target.halt()
                print("  Target halted")
            
            # Yield the connection object
            yield STM32Connection(session, target, self.symbols, self.line_info)
            
        except Exception as e:
            print(f"  Connection failed: {e}")
            print(f"  Troubleshooting tips:")
            print(f"    - Make sure ST-Link drivers are installed")
            print(f"    - Check that ST-Link is connected via USB")
            print(f"    - Verify target MCU has power")
            print(f"    - Try: pyocd list  (to see available probes)")
            raise
        
        finally:
            if session:
                session.close()
                print("\nDisconnected from target")

    def load_line_info(self, elf_path: str) -> bool:
        """
        Parse ELF file and extract line number information (DWARF debug info)
        
        Args:
            elf_path: Path to the .elf file
        """
        print(f"[Loading line info]")
        
        try:
            with open(elf_path, 'rb') as f:
                print(f"  Loading {elf_path}")
                elf = ELFFile(f)
                
                if not elf.has_dwarf_info():
                    print("  Warning: ELF file has no DWARF debug info")
                    return False
                
                dwarf_info = elf.get_dwarf_info()
                
                # Iterate through all compilation units
                for CU in dwarf_info.iter_CUs():
                    # Get the line program for this CU
                    line_program = dwarf_info.line_program_for_CU(CU)
                    if line_program is None:
                        continue
                    
                    for entry in line_program.get_entries():
                        if entry.state is None:
                            continue
                        
                        # Store mapping from (file, line) to address
                        file_entry = line_program['file_entry'][entry.state.file - 1]
                        file_name = file_entry.name.decode('utf-8', errors='ignore')
                        
                        # Get directory if available
                        if file_entry.dir_index != 0:
                            dir_entry = line_program['include_directory'][file_entry.dir_index - 1]
                            dir_name = dir_entry.decode('utf-8', errors='ignore')
                            full_path = f"{dir_name}/{file_name}"
                        else:
                            full_path = file_name
                        
                        line = entry.state.line
                        address = entry.state.address
                        
                        if address > 0:
                            self.line_info[(full_path, line)] = address
                
                print(f"Loaded {len(self.line_info)} line entries")
                return True
                
        except Exception as e:
            print(f"  Failed to load line info: {e}")
            return False

    def list_source_files(self, pattern=None):
        """
        List all source files found in the ELF debug info
        
        Args:
            pattern: Optional string to filter file names
            
        Returns:
            List of unique source file paths
        """
        if not self.line_info:
            print("No line information loaded. Make sure ELF was compiled with -g")
            return []
        
        # Extract unique file paths
        source_files = set()
        for (file_path, _), _ in self.line_info.items():
            source_files.add(file_path)
        
        # Filter if pattern provided
        if pattern:
            source_files = {f for f in source_files if pattern.lower() in f.lower()}
        
        # Sort for consistent output
        source_files = sorted(source_files)
        
        print(f"\nFound {len(source_files)} source files" + (f" matching '{pattern}'" if pattern else ""))
        for i, file_path in enumerate(source_files, 1):
            print(f"  {i:3d}. {file_path}")
        
        return source_files 

def main() -> None:
    injector = STM32FaultInjector(
        target_type='stm32f411ceux',
        elf_path='../stm32/build/stm32.elf'
    )
    
    with injector.connect() as conn:
        print("[Reset Target]")
        conn.reset(halt=True)

        symbols = conn.list_symbols()
        injector.list_source_files()

        conn.run_until_line('main.cpp', 132)
        conn.resume()


if __name__ == "__main__":
    main()
        # Example 2: Dump arena structure
        # print("\n" + "="*60)
        # print("\nExample 2: Dump Arena State")
        # conn.dump_arena('main_arena')
        
        # Example 3: Inject fault into specific arena field
        # print("\n" + "="*60)
        # print("\nExample 3: Corrupt Arena Offset")
        # conn.inject_arena_field_fault('main_arena', 'offset', bit_position=16)
        
        # # Example 4: Run comprehensive fault campaign with logging
        # print("\n" + "="*60)
        # print("\nExample 4: Arena Fault Campaign")
        
        # fault_scenarios = [
        #     ('data', 8, 'Corrupt data pointer - low byte'),
        #     ('offset', 31, 'Corrupt offset - sign bit flip'),
        #     ('capacity', 0, 'Corrupt capacity - LSB flip'),
        #     ('last_allocation', 16, 'Corrupt last_allocation pointer'),
        #     ('offset', 16, 'Corrupt offset - middle bits'),
        # ]
        
        # results = conn.arena_fault_campaign(
        #     'main_arena',
        #     fault_scenarios,
        #     log_file='fault_injection_results.csv'
        # )
        
        # # Example 5: Reset and resume
