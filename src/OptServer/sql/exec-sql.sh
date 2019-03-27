mysql -h db_install_ip -uroot -proot -e "drop database if exists db_dcache_relation; create database db_dcache_relation"

mysql -h db_install_ip -uroot -proot --default-character-set=utf8 db_dcache_relation < db_dcache_relation.sql
