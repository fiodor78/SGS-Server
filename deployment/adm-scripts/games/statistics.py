#!/usr/bin/env python

import time
import urllib
import socket
import telnetlib
import re
import pymysql
import ConfigParser
import sys

Config = ConfigParser.ConfigParser()
Config.read("/opt/servers/bin/GlobalConf/server.cfg")

current_timestamp = int(time.time())
date_created = current_timestamp - (current_timestamp % 300)
time.sleep(3)

METRIC_NAME = 0
MIN_VALUE = 1
MAX_VALUE = 2
SUM = 3
SAMPLE_COUNT = 4
UNITS = 6

def get_instance_info(url):
        try:
                return str(urllib.urlopen(url).read())
        except:
                sys.exit("get instance info")

def console_connect(host, port):
        try:
                return telnetlib.Telnet(host, port)
        except:
                sys.exit("console connect")

def mysql_connect(db_host, db_user, password, database):
    try:
        conn = pymysql.connect(host=db_host, port=3306, user=db_user, passwd=password, db=database)
        return conn
    except:
        sys.exit("mysql connect")

def get_console_port():
    f = open('/opt/servers/bin/GlobalConf/server.cfg')
    content = f.read()
    consoleport = re.search('game=(.*)',content).group(0)
    return re.search('(?<=consoleport:)\d+',consoleport).group(0)

def get_monitoring_settings(section, option):
    try:
        return Config.get(section,option)
    except:
        sys.exit("get monitring settings")


game_name = get_monitoring_settings('Misc','Game')
host = get_monitoring_settings('Monitoring','host')
database = get_monitoring_settings('Monitoring','database')
table = get_monitoring_settings('Monitoring','table')
passwd = get_monitoring_settings('Monitoring','passwd')
user = get_monitoring_settings('Monitoring','user')

instance_id = get_instance_info("http://169.254.169.254/latest/meta-data/instance-id")
instance_type = get_instance_info("http://169.254.169.254/latest/meta-data/instance-type")
instance_ip = get_instance_info("http://169.254.169.254/latest/meta-data/local-ipv4")
consoleport = get_console_port()
console = console_connect(instance_ip, consoleport)
console.read_until("base")
console.write("logic stats csv=1 interval=300")
response = console.read_until("---END---").replace("---END---", "")
response = response.replace("Main Thread","Group 0")
response = re.compile("\r\s\-* Group \d \-*\s").split(response)
console.close()
result = {}
query_data = []
for item in response:
        group_stats = item.split("\r\n")
        if len(group_stats) == 1:
                continue
        metrics_count = int(group_stats[1])
        if int(group_stats[1]) == 0:
                continue
        for i in range(2,metrics_count+2):
                msg = group_stats[i].split(";")
                if result.has_key(msg[METRIC_NAME]):
                        if msg[UNITS] != '1' and msg[UNITS] != '-':
                                if (float(result[msg[METRIC_NAME]][MIN_VALUE]) > float(msg[MIN_VALUE]) and int(msg[SAMPLE_COUNT]) != 0) or (int(msg[SAMPLE_COUNT]) != 0 and  int(result[msg[METRIC_NAME]][SAMPLE_COUNT]) == 0):
                                        result[msg[METRIC_NAME]][MIN_VALUE] = round(float(msg[MIN_VALUE]),3)
                                if float(result[msg[METRIC_NAME]][MAX_VALUE]) < float(msg[MAX_VALUE]):
                                        result[msg[METRIC_NAME]][MAX_VALUE] = round(float(msg[MAX_VALUE]),3)
                                result[msg[METRIC_NAME]][SUM] += round(float(msg[SUM]),3)
                        else:
                                result[msg[METRIC_NAME]][MIN_VALUE] += round(float(msg[MIN_VALUE]),3)
                                result[msg[METRIC_NAME]][MAX_VALUE] += round(float(msg[MAX_VALUE]),3)
                                if int(msg[SAMPLE_COUNT]) != 0:
                                        result[msg[METRIC_NAME]][SUM] += round(float(msg[SUM]) / int(msg[SAMPLE_COUNT]),3)
                        result[msg[METRIC_NAME]][SAMPLE_COUNT] += int(msg[SAMPLE_COUNT])
                        result[msg[METRIC_NAME]][UNITS] = msg[UNITS]
                else:
                        result[msg[METRIC_NAME]] = {}
                        result[msg[METRIC_NAME]][MAX_VALUE] = round(float(msg[MAX_VALUE]),3)
                        result[msg[METRIC_NAME]][MIN_VALUE] = round(float(msg[MIN_VALUE]),3)
                        if msg[UNITS] == '1' or msg[UNITS] == '-':
                                if int(msg[SAMPLE_COUNT]) != 0:
                                        result[msg[METRIC_NAME]][SUM] = round(float(msg[SUM]) / int(msg[SAMPLE_COUNT]),3)
                                else:
                                        result[msg[METRIC_NAME]][SUM] = 0
                        else:
                                result[msg[METRIC_NAME]][SUM] = round(float(msg[SUM]),3)
                        result[msg[METRIC_NAME]][SAMPLE_COUNT] = int(msg[SAMPLE_COUNT])
                        result[msg[METRIC_NAME]][UNITS] = msg[UNITS]

for key, value in result.iteritems():
        row = instance_id, key, game_name, instance_type, value[MIN_VALUE], value[MAX_VALUE], value[SUM],value[SAMPLE_COUNT], date_created, value[UNITS]
        query_data.append(row)
conn = mysql_connect(host, user, passwd, database)
cursor = conn.cursor()
cursor.executemany("insert into logic_stats(instance_id, name,app_id,instance_type, min, max, sum, sample_count, timestamp, units) values(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)", query_data)
conn.commit()
cursor.close()
conn.close()

