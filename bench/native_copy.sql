truncate customers_csv;

copy customers_csv from 'data/customers-1000000.csv' with (
  format csv,
  header true
);
