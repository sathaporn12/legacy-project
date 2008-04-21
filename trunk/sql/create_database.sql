GRANT USAGE ON *.* TO 'legacy'@'localhost' IDENTIFIED BY 'legacy' WITH MAX_QUERIES_PER_HOUR 0 MAX_CONNECTIONS_PER_HOUR 0 MAX_UPDATES_PER_HOUR 0;

CREATE DATABASE `legacy` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

CREATE DATABASE `characters` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

CREATE DATABASE `realmd` DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;

GRANT ALL PRIVILEGES ON `legacy`.* TO 'legacy'@'localhost' WITH GRANT OPTION;

GRANT ALL PRIVILEGES ON `characters`.* TO 'legacy'@'localhost' WITH GRANT OPTION;

GRANT ALL PRIVILEGES ON `realmd`.* TO 'legacy'@'localhost' WITH GRANT OPTION;
