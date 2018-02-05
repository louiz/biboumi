#!/bin/sh

sqlite3_args=$@

function dump_table {
    table=$1
    columns=$2
    echo ".mode insert $table
.output $table.sql
select $columns from $table;" | sqlite3 $sqlite3_args
}

dump_table "roster" "local, remote"

dump_table "ircserveroptions_" "id_, owner_, server_, pass_, afterconnectioncommand_, tlsports_, ports_, username_, realname_, verifycert_, trustedfingerprint_, encodingout_, encodingin_, maxhistorylength_"

dump_table "ircchanneloptions_" "id_, owner_, server_, channel_, encodingout_, encodingin_, maxhistorylength_, persistent_, recordhistory_"

dump_table "globaloptions_" "id_, owner_, maxhistorylength_, recordhistory_, persistent_"

dump_table "muclogline_" "id_, uuid_, owner_, ircchanname_, ircservername_, date_, body_, nick_"
