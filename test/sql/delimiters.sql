-- semicolon delimiter
SELECT csv_agg(x, ';') AS body
FROM   projects x;

-- pipe delimiter
SELECT csv_agg(x, '|') AS body
FROM   projects x;

-- tab delimiter
SELECT csv_agg(x, E'\t') AS body
FROM   projects x;

-- newline is forbidden as delimiter
SELECT csv_agg(x, E'\n') AS body
FROM   projects x;

-- double quote is forbidden as delimiter
SELECT csv_agg(x, '"') AS body
FROM   projects x;

-- carriage return is forbidden as delimiter
SELECT csv_agg(x, E'\r') AS body
FROM   projects x;
