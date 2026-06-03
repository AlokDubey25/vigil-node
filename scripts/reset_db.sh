#!/bin/bash

# ^ Shebang

# Its Shell Script or Bash Script as .sh
# scripts/reset_db.sh
# it'll take new data everytime and add it to db everytime
# always Unique ID and Error if null


echo "[reset] removing vigil.db..."    # DB can end up in either place depending on where we ran binary


rm -f build/vigil.db
rm -f vigil.db

echo "[reset] done. run ./build/vigil to recreate fresh."