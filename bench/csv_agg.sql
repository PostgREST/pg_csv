\set lim random(1000, 2000)

select csv_agg(t) from (
  select * from student_emotion_assessments limit :lim
) as t;
