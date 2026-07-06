-- can read an inline value
select id, name
from csv_read(null::projects, E'1,IOS,4\n2,"Win""dows",4') where id = 2;

-- can read a CSV file in full
select count(*) = 100 as all_read
from csv_read(
  null::customers,
  pg_read_file('data/customers-100.csv')
);

-- can insert only some columns of a CSV
insert into customers
select "First Name", "Company", "Website"
from csv_read(
  null::customers,
  pg_read_file('data/customers-100.csv')
)
where "Index" = '4';
\echo

-- check inserted columns
select * from customers;

-- clear inserted
truncate customers;

-- rejects NULL input
select * from csv_read(null::projects, NULL::text);
\echo

-- rejects empty row types
select * from csv_read(null::empty, 'id,name');
\echo

-- rejects rows with the wrong number of columns
select * from csv_read(
  null::projects,
  E'1,IOS,4\n2,Windows'
);
\echo

-- rejects rows wider than PostgreSQL's column limit
select * from csv_read(
  null::projects,
  array_to_string(array_fill('x'::text, ARRAY[1601]), ',')
);
\echo

-- fails at parse errors
select * from csv_read(null::projects, E'1,""IOS,4');
