CREATE TABLE IF NOT EXISTS contacts
(
    id             integer not null
        constraint contacts_id_pk primary key autoincrement,
    firstname      varchar(128),
    lastname       varchar(128),
    bic            varchar(11),
    iban           varchar(32),
    bank_name      varchar(128),
    bank_code      varchar(10),
    account_number varchar(10),
    created_at     datetime default null,
    updated_at     datetime default null
);
CREATE INDEX IF NOT EXISTS contacts_firstname_index on contacts (firstname asc);
CREATE INDEX IF NOT EXISTS contacts_lastname_index on contacts (lastname asc);
CREATE INDEX IF NOT EXISTS contacts_bank_name_index on contacts (bank_name asc);

CREATE TRIGGER IF NOT EXISTS trg_insert_contacts
    after insert
    on contacts
begin
    update contacts set created_at = (datetime('now', 'localtime')) where id = new.id#
end;

CREATE TRIGGER IF NOT EXISTS trg_update_contacts
    after update
    on contacts
begin
    update contacts set updated_at = (datetime('now', 'localtime')) where id = new.id#
end;

create table IF NOT EXISTS categories
(
    id                 integer not null
        constraint migrations_id_pk primary key autoincrement,
    name               varchar,
    parent_id          integer,
    sub_categorie_name varchar
);
CREATE INDEX IF NOT EXISTS categories_name_index on categories (name asc);
CREATE INDEX IF NOT EXISTS categories_parent_id_index on categories (parent_id asc);

create table IF NOT EXISTS accounts
(
    id                 integer not null
        constraint accounts_id_pk primary key autoincrement,
    `type`             integer,
    unique_id          integer,
    backend_name       varchar,
    owner_name         varchar,
    account_name       varchar,
    currency           varchar,
    memo               varchar,
    iban               varchar,
    bic                varchar,
    country            varchar,
    bank_code          varchar,
    bank_name          varchar,
    branch_id          varchar,
    account_number     varchar,
    sub_account_number varchar,
    balance            double
);
CREATE INDEX IF NOT EXISTS accounts_unique_id_index on accounts (unique_id asc);
CREATE INDEX IF NOT EXISTS accounts_iban_index on accounts (iban asc);
CREATE INDEX IF NOT EXISTS accounts_balance_index on accounts (iban asc);

create table IF NOT EXISTS transaction_categories
(
    id        integer not null
        constraint transaction_categories_id_pk primary key autoincrement,
    parent_id integer null,
    name      varchar

);
CREATE INDEX IF NOT EXISTS transaction_categories_parent_id_index on transaction_categories (parent_id);
CREATE INDEX IF NOT EXISTS transaction_categories_name_index on transaction_categories (name asc);

CREATE TABLE IF NOT EXISTS transactions
(
    id                          integer not null
        constraint transactions_id_pk primary key autoincrement,
    account_id                  integer,
    `type`                      integer,
    sub_type                    integer,
    command                     integer,
    status                      integer,
    unique_account_id           unsigned integer,
    unique_id                   unsigned integer,
    ref_unique_id               unsigned integer,
    id_for_application          unsigned integer,
    string_id_for_application   varchar,
    session_id                  unsigned integer,
    group_id                    unsigned integer,
    fi_id                       varchar,
    local_iban                  varchar,
    local_bic                   varchar,
    local_country               varchar,
    local_bank_code             varchar,
    local_branch_id             varchar,
    local_account_number        varchar,
    local_suffix                varchar,
    local_name                  varchar,
    remote_country              varchar,
    remote_bank_code            varchar,
    remote_branch_id            varchar,
    remote_account_number       varchar,
    remote_suffix               varchar,
    remote_iban                 varchar,
    remote_bic                  varchar,
    remote_name                 varchar,
    `date`                      date,
    valuta_date                 date,
    value                       double,
    currency                    varchar,
    fees                        double,
    transaction_code            integer,
    transaction_text            varchar,
    transaction_key             varchar,
    text_key                    integer,
    primanota                   varchar,
    purpose                     text,
    `category`                  varchar,
    customer_reference          varchar,
    bank_reference              varchar,
    end_to_end_reference        varchar,
    creditor_scheme_id          varchar,
    originator_id               varchar,
    mandate_id                  varchar,
    mandate_date                date,
    mandate_debitor_name        varchar,
    original_creditor_scheme_id varchar,
    original_mandate_id         varchar,
    original_creditor_name      varchar,
    `sequence`                  integer,
    charge                      integer,
    remote_addr_street          varchar,
    remote_addr_zipcode         varchar,
    remote_addr_city            varchar,
    remote_addr_phone           varchar,
    period                      integer,
    `cycle`                     unsigned integer,
    execution_day               unsigned integer,
    first_date                  date,
    last_date                   date,
    next_date                   date,
    unit_id                     varchar,
    unit_id_name_space          varchar,
    ticker_symbol               varchar,
    units                       double,
    unit_price_value            double,
    unit_price_date             date,
    commission_value            double,
    memo                        text,
    `hash`                      varchar,
    FOREIGN KEY (account_id) REFERENCES accounts (id)
);
CREATE INDEX IF NOT EXISTS transactions_account_id_index on transactions (account_id asc);
CREATE INDEX IF NOT EXISTS transactions_unique_account_id_index on transactions (unique_account_id asc);
CREATE INDEX IF NOT EXISTS transactions_unique_id_index on transactions (unique_id asc);
CREATE INDEX IF NOT EXISTS transactions_ref_unique_id_index on transactions (ref_unique_id asc);
CREATE INDEX IF NOT EXISTS transactions_id_for_application_index on transactions (id_for_application asc);
CREATE INDEX IF NOT EXISTS transactions_category_index on transactions (category asc);
CREATE INDEX IF NOT EXISTS transactions_date_index on transactions (`date` desc);
CREATE INDEX IF NOT EXISTS transactions_valuta_date_index on transactions (valuta_date desc);
CREATE INDEX IF NOT EXISTS transactions_mandate_date_index on transactions (mandate_date desc);
CREATE INDEX IF NOT EXISTS transactions_first_date_index on transactions (first_date desc);
CREATE INDEX IF NOT EXISTS transactions_last_date_index on transactions (last_date desc);
CREATE INDEX IF NOT EXISTS transactions_next_date_index on transactions (next_date desc);
CREATE INDEX IF NOT EXISTS transactions_unit_price_date_index on transactions (unit_price_date desc);

CREATE TABLE IF NOT EXISTS migrations
(
    id         integer not null
        constraint migrations_id_pk primary key autoincrement,
    name       varchar not null,
    migrated   tinyint null default 0,
    created_at datetime     default null,
    UNIQUE (name)
);
CREATE UNIQUE INDEX IF NOT EXISTS migrations_name_unique_index on migrations (name asc);
CREATE INDEX IF NOT EXISTS migrations_migrated_index on migrations (migrated asc);

CREATE TRIGGER IF NOT EXISTS trg_insert_migrations
    after insert
    on migrations
begin
    update migrations
    set migrated   = 1,
        created_at = (datetime('now', 'localtime'))
    where id = new.id#
end;

INSERT OR IGNORE INTO migrations (name)
VALUES ('contacts'),
       ('categories'),
       ('accounts'),
       ('transaction_categories'),
       ('transactions');
