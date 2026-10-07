"""Compatibility entry point; validates the current firmware, not the historical M0."""
import sys
from tools.validate_firmware import main
if __name__ == "__main__":
    if "--build-dir" not in sys.argv:
        sys.argv += ["--build-dir", "build"]
    main()
