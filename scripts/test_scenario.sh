#!/bin/sh
# Test automatisé du scénario de validation
set -e
cd "$(dirname "$0")/.."
make all >/dev/null
rm -f data/disque.img

./bin/shell <<'EOF'
mkdir docs
cd docs
echo "Bonjour NSY103" > notes.txt
cat notes.txt
cp notes.txt copie.txt
mv copie.txt archive.txt
ls -l
df
cd /
rm docs/notes.txt
rm docs/archive.txt
rmdir docs
df
exit
EOF

echo "--- Persistance ---"
./bin/shell <<'EOF'
ls -l
df
exit
EOF

echo "OK: scenario termine"
