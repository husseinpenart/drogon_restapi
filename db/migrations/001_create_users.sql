CREATE TABLE public.UserCase
(
    id       uuid PRIMARY KEY,
    name     TEXT         NOT NULL,
    email    VARCHAR(100) NOT NULL,
    username VARCHAR(100) NOT NULL,
    password VARCHAR(255) NOT NULL
)