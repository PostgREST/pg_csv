-- this is done to avoid failing on a pure psql change that happened on postgres 16
-- on pg <= 15 the BOM output adds one extra space, on pg 16 it doesn't
\pset format unaligned
\pset tuples_only on
\echo

-- include BOM (byte-order mark)
SELECT csv_agg(x, csv_options(bom := true)) AS body
FROM   projects x;
\echo

-- include BOM with custom delimiter
SELECT csv_agg(x, csv_options(delimiter := ';', bom := true)) AS body
FROM   projects x;
