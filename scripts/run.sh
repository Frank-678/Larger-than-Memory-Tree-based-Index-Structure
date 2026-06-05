#!/bin/bash

# ============================================================================
# LTM B+ Tree - Complete Data Generation & Visualization Pipeline
# ============================================================================

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
DATA_DIR="$PROJECT_ROOT/data"
BIN="$BUILD_DIR/ltm_index"

echo ""
echo "=============================================================="
echo "   LTM B+ Tree - Data Generation & Visualization"
echo "=============================================================="
echo ""

# Step 1: Check environment
echo "[1] Checking environment..."
if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake not found!"
    exit 1
fi
if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo "ERROR: C++ compiler not found!"
    exit 1
fi
if ! command -v python3 &> /dev/null; then
    echo "ERROR: Python3 not found!"
    exit 1
fi
echo "    OK: All dependencies found"
echo ""

# Step 2: Create data directory
echo "[2] Creating data directories..."
mkdir -p "$DATA_DIR"
mkdir -p "$BUILD_DIR"
echo "    OK: Directories ready"
echo ""

# Step 3: Build C++ project
echo "[3] Building C++ project..."
cd "$BUILD_DIR"
cmake ..
make -j$(nproc)
echo "    OK: Build completed"
echo ""

# Step 4: Run tests and data generation
echo "[4] Running tests and generating data..."
cd "$PROJECT_ROOT"
"$BIN"
echo ""
echo "    OK: Data generation completed"
echo "    Generated files:"
ls -lh "$DATA_DIR"/*.csv 2>/dev/null || echo "    (checking files...)"
echo ""

# Step 5: Generate visualizations
echo "[5] Generating visualization charts..."
python3 "$PROJECT_ROOT/benchmark_visualizer.py"
echo ""
echo "    OK: Visualization completed"
echo "    Generated charts:"
ls -lh "$DATA_DIR"/*.png 2>/dev/null || echo "    (checking files...)"
echo ""

# Step 6: Summary
echo "=============================================================="
echo "   ✓ Pipeline completed successfully!"
echo "=============================================================="
echo ""
echo "📊 Results Location: $DATA_DIR"
echo ""
echo "Data Files:"
ls -1 "$DATA_DIR"/*.csv 2>/dev/null | sed 's|.*\/||' | sed 's/^/   - /'
echo ""
echo "Charts:"
ls -1 "$DATA_DIR"/*.png 2>/dev/null | sed 's|.*\/||' | sed 's/^/   - /'
echo ""
echo "=============================================================="
echo ""
