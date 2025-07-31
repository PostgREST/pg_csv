create extension if not exists pg_csv;

create type gender_enum          as enum ('female', 'male', 'non_binary', 'prefer_not_to_say');
create type attachment_enum      as enum ('secure', 'anxious', 'avoidant', 'fearful');
create type regulation_strategy  as enum ('cognitive_reappraisal', 'suppression', 'rumination',
                                          'problem_solving', 'distraction', 'other');

create table student_emotion_assessments (
    -- identifiers
    assessment_id        bigserial primary key,
    student_uuid         uuid        not null,
    institution_id       int         not null,

    -- demographics
    gender               gender_enum not null,
    birth_date           date        not null,
    nationality          text        not null,
    socioeconomic_level  text        not null,

    -- academic context
    faculty              text         not null,
    degree_program       text         not null,
    year_of_study        smallint     not null check (year_of_study between 1 and 7),
    current_gpa          numeric(3,2) not null check (current_gpa between 0 and 4),
    credits_completed    int          not null check (credits_completed >= 0),
    enrollment_status    boolean      not null default true, -- true = active student

    -- attachment style
    attachment_style     attachment_enum not null,
    attachment_score_anxiety  numeric(4,2) not null check (attachment_score_anxiety  between 1 and 7),
    attachment_score_avoidant numeric(4,2) not null check (attachment_score_avoidant between 1 and 7),

    -- difficulties in emotion regulation scale (ders-18) sub-scores
    ders_non_acceptance  smallint not null check (ders_non_acceptance between 6 and 30),
    ders_goals           smallint not null check (ders_goals between 5 and 25),
    ders_impulse         smallint not null check (ders_impulse between 6 and 30),
    ders_awareness       smallint not null check (ders_awareness between 6 and 30),
    ders_strategy        smallint not null check (ders_strategy between 8 and 40),
    ders_clarity         smallint not null check (ders_clarity between 5 and 25),
    ders_total           smallint generated always as
                         (ders_non_acceptance + ders_goals + ders_impulse +
                          ders_awareness + ders_strategy + ders_clarity) stored,

    -- emotion-regulation strategy prevalence (likert 1-5)
    uses_reappraisal     smallint not null check (uses_reappraisal between 1 and 5),
    uses_suppression     smallint not null check (uses_suppression between 1 and 5),
    uses_rumination      smallint not null check (uses_rumination between 1 and 5),
    predominant_strategy regulation_strategy not null,

    -- well-being & mental-health screeners
    perceived_stress     smallint not null check (perceived_stress between 0 and 40),
    anxiety_score_gad7   smallint not null check (anxiety_score_gad7 between 0 and 21),
    depression_score_phq9 smallint not null check (depression_score_phq9 between 0 and 27),

    -- environmental variables
    living_with_family   boolean not null,
    weekly_work_hours    smallint not null check (weekly_work_hours between 0 and 60),
    social_support_index smallint not null check (social_support_index between 12 and 84),

    -- audit fields
    administered_by      text  not null, -- name/id of interviewer or system
    collected_at         timestamptz  not null default now(),
    updated_at           timestamptz  not null default now(),
    constraint updated_at_future check (updated_at <= now())
);

INSERT INTO student_emotion_assessments (
    student_uuid, institution_id, gender, birth_date, nationality, socioeconomic_level,
    faculty, degree_program, year_of_study, current_gpa, credits_completed, enrollment_status,
    attachment_style, attachment_score_anxiety, attachment_score_avoidant,
    ders_non_acceptance, ders_goals, ders_impulse, ders_awareness, ders_strategy, ders_clarity,
    uses_reappraisal, uses_suppression, uses_rumination, predominant_strategy,
    perceived_stress, anxiety_score_gad7, depression_score_phq9,
    living_with_family, weekly_work_hours, social_support_index,
    administered_by
)
SELECT
    gen_random_uuid(),                                     -- student_uuid
    1 + (i % 5),                                           -- institution_id 1-5
    CASE (i % 4)
         WHEN 0 THEN 'female'
         WHEN 1 THEN 'male'
         WHEN 2 THEN 'non_binary'
         ELSE 'prefer_not_to_say'
    END::gender_enum,
    (CURRENT_DATE - ((18 + (i % 10)) * INTERVAL '1 year'))::date,
    'Country ' || i,
    CASE WHEN i % 3 = 0 THEN 'alto'
         WHEN i % 3 = 1 THEN 'medio'
         ELSE 'bajo'
    END,
    CASE WHEN i % 2 = 0 THEN 'Psychology' ELSE 'Engineering' END,
    CASE WHEN i % 2 = 0 THEN 'BSc' ELSE 'BA' END,
    (i % 7) + 1,
    round((random()*4)::numeric, 2)::numeric(3,2),         -- GPA 0-4.00
    (i * 10) % 200,
    TRUE,
    CASE (i % 4)
         WHEN 0 THEN 'secure'
         WHEN 1 THEN 'anxious'
         WHEN 2 THEN 'avoidant'
         ELSE 'fearful'
    END::attachment_enum,
    3 + (i % 4),
    3 + ((i + 2) % 4),
    10 +  (i       % 15),     -- ders_non_acceptance
    10 + ((i+1) % 15),        -- ders_goals
    10 + ((i+2) % 15),        -- ders_impulse
    10 + ((i+3) % 15),        -- ders_awareness
    15 +  (i      % 10),      -- ders_strategy
    5  + ((i+4) % 21),        -- ders_clarity  (range 5-25)
    1 + (i % 5),              -- uses_reappraisal
    1 + ((i+1) % 4),          -- uses_suppression
    1 + ((i+2) % 3),          -- uses_rumination
    CASE (i % 6)
         WHEN 0 THEN 'cognitive_reappraisal'
         WHEN 1 THEN 'suppression'
         WHEN 2 THEN 'rumination'
         WHEN 3 THEN 'problem_solving'
         WHEN 4 THEN 'distraction'
         ELSE 'other'
    END::regulation_strategy,
    10 + (i % 20),
    2  + (i % 5),
    3  + (i % 9),
    (i % 2 = 0),
    (i % 61),
    20 + (i % 50),
    'seed_script'
FROM generate_series(1, 3000) AS s(i);
