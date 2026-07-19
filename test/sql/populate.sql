select * from csv_populate_recordset(null::projects, E'id,name,client_id\n1,IOS,4', true);

select * from csv_populate_recordset(null::projects, E'1,IOS,4');
