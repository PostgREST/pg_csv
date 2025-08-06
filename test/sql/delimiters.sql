-- semicolon delimiter
SELECT csv_agg(x, csv_options(';')) AS body
FROM   projects x;

-- pipe delimiter, named params work too
SELECT csv_agg(x, csv_options(delimiter := '|')) AS body
FROM   projects x;

-- tab delimiter
SELECT csv_agg(x, csv_options(E'\t')) AS body
FROM   projects x;

-- newline is forbidden as delimiter
SELECT csv_agg(x, csv_options(E'\n')) AS body
FROM   projects x;

-- double quote is forbidden as delimiter
SELECT csv_agg(x, csv_options('"')) AS body
FROM   projects x;

-- carriage return is forbidden as delimiter
SELECT csv_agg(x, csv_options(E'\r')) AS body
FROM   projects x;
