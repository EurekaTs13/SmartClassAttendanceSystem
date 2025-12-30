-- 课程表
CREATE TABLE IF NOT EXISTS course (
    course_id TEXT PRIMARY KEY,
    course_name TEXT NOT NULL
);

-- 学生表
CREATE TABLE IF NOT EXISTS student (
    student_id TEXT PRIMARY KEY,
    student_name TEXT NOT NULL
);

-- 考勤表
CREATE TABLE IF NOT EXISTS attendance (
    course_id TEXT,
    student_id TEXT,
    week INTEGER,
    status INTEGER DEFAULT 0,
    PRIMARY KEY (course_id, student_id, week)
);