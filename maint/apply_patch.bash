#! /usr/bin/env bash

echo "Applying patch to src/hwloc..."
cd src/hwloc && git am --3way ../../maint/patches/pre/hwloc/*.patch
