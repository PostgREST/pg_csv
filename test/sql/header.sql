-- header
SELECT csv_agg(x, csv_options(header:=true)) AS body
FROM   projects x;

-- no header
SELECT csv_agg(x, csv_options(header:=false)) AS body
FROM   projects x;

-- no header with delimiter
SELECT csv_agg(x, csv_options(delimiter:='|', header:=false)) AS body
FROM   projects x;

-- see bom.sql for an explanation of these settings
\pset format unaligned
\pset tuples_only on
\echo

-- no header with delimiter and BOM
SELECT csv_agg(x, csv_options(delimiter:='|', header:=false, bom := true)) AS body
FROM   projects x;
\echo

\pset format aligned
\pset tuples_only off
