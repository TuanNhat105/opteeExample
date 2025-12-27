#!/bin/bash
# Create symbolic links from .cc to .cpp for LevelDB sources
# This allows OP-TEE build system to compile them

set -e

LEVELDB_ROOT="../leveldb"
LINK_DIR="leveldb_cpp"

echo "Creating symbolic links for LevelDB sources..."

# Remove old links if exist
rm -rf "$LINK_DIR"
mkdir -p "$LINK_DIR/db"
mkdir -p "$LINK_DIR/table"
mkdir -p "$LINK_DIR/util"

# DB files
for file in builder db_impl db_iter dbformat filename log_reader log_writer memtable repair table_cache version_edit version_set write_batch; do
    ln -sf "../../${LEVELDB_ROOT}/db/${file}.cc" "${LINK_DIR}/db/${file}.cpp"
done

# Table files
for file in block block_builder filter_block format iterator merger table table_builder two_level_iterator; do
    ln -sf "../../${LEVELDB_ROOT}/table/${file}.cc" "${LINK_DIR}/table/${file}.cpp"
done

# Util files
# Note: env_posix is created but NOT included in sub.mk (we use custom RingBufferEnv instead)
for file in arena bloom cache coding comparator crc32c env env_posix filter_policy hash histogram logging options status; do
    ln -sf "../../${LEVELDB_ROOT}/util/${file}.cc" "${LINK_DIR}/util/${file}.cpp"
done

echo "Symbolic links created in $LINK_DIR/"
ls -la "$LINK_DIR"/*/*.cpp | wc -l
echo "files linked"
