truncate customers_csv;

insert into customers_csv
select *
from csv_read(
  null::customers_csv,
  pg_read_file('data/customers-1000000.csv')
);
