CREATE TABLE projects
( id integer
, name text
, project_name text
, client_id integer
, subclient_id int
);
-- ensure these dropped column cases are tested
ALTER TABLE projects DROP COLUMN project_name;
ALTER TABLE projects DROP COLUMN subclient_id;

INSERT INTO projects VALUES (1, 'Windows 7', 1);
INSERT INTO projects VALUES (2, 'has,comma', 1);
INSERT INTO projects VALUES (NULL, NULL, NULL);
INSERT INTO projects VALUES (4, 'OSX', 2);
INSERT INTO projects VALUES (NULL, 'has"quote', NULL);
INSERT INTO projects VALUES (5, 'has,comma and "quote"', 7);
INSERT INTO projects VALUES (6, E'has \n LF', 7);
INSERT INTO projects VALUES (7, E'has \r CR', 8);
INSERT INTO projects VALUES (8, E'has \r\n CRLF"', 8);

create extension if not exists pg_csv;

CREATE TABLE nasty (
  "unusual"",names" INTEGER GENERATED ALWAYS AS IDENTITY,
  text TEXT
);
INSERT INTO nasty (text) VALUES ('test');
