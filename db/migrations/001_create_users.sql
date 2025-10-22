CREATE TABLE users
(
    id       INT PRIMARY KEY NOT NULL,
    name     TEXT            NOT NULL,
    email    VARCHAR(100)    NOT NULL,
    username VARCHAR(100),
    password VARCHAR(255)
)