-- custom null string
SELECT csv_agg(x, csv_options(nullstr:='<null>')) AS body
FROM   projects x;

-- custom null string with no header
SELECT csv_agg(x, csv_options(nullstr:='NULL', header:=false)) AS body
FROM   projects x;

-- custom null string with no header and delimiter
SELECT csv_agg(x, csv_options(nullstr:='~', delimiter:='|', header:=false)) AS body
FROM   projects x;
