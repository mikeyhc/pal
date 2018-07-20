CREATE TABLE properties (
        name STRNG PRIMARY KEY,
        value STRING NOT NULL
);

CREATE TABLE crew (
        name STRING,
        status STRING
);

CREATE TABLE modules (
        name STRING PRIMARY KEY,
        status STRING
);

CREATE TABLE features (
        name STRING PRIMARY KEY,
        status STRING
);

CREATE TABLE cargo (
        name STRING PRIMARY KEY,
        count NUMBER
);
