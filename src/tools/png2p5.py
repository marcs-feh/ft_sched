#!/usr/bin/env python3

"""
Converts a PNG file (or any image readable by Pillow) to a
binary PGM (P5) file.
"""

import sys
import argparse
from PIL import Image

def convert_png_to_pgm(input_path, output_path):
    """
    Opens a PNG file, converts it to 8-bit grayscale, and saves
    it as a binary PGM (P5) file.
    """
    try:
        # Open the image using Pillow
        # This handles PNG, JPEG, BMP, etc.
        with Image.open(input_path) as img:
            grayscale_img = img.convert('L')
            
            width, height = grayscale_img.size
            
            pixel_data = grayscale_img.tobytes()

            # --- Create the PGM P5 Header ---
            magic = b'P5\n'
            
            dimensions = f"{width} {height}\n".encode('ascii')
            
            maxval = b'255\n'
            
            header = magic + dimensions + maxval

            with open(output_path, 'wb') as f:
                f.write(header)
                f.write(pixel_data)
                
            print(f"Successfully converted {input_path} to {output_path} (P5 binary PGM).")

    except FileNotFoundError:
        print(f"Error: Input file not found at {input_path}", file=sys.stderr)
        sys.exit(1)
    except IOError as e:
        print(f"Error writing to {output_path}: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert PNG to PGM (P5 binary format).",
        epilog="Example: python png_to_pgm.py lena.png lena.pgm"
    )
    parser.add_argument("input_png", help="The path to the input PNG file.")
    parser.add_argument("output_pgm", help="The path for the output PGM file.")
    
    args = parser.parse_args()
    
    convert_png_to_pgm(args.input_png, args.output_pgm)
