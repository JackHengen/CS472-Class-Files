import ntplib

res = ntplib.NTPClient().request('pool.ntp.org')
ntp_time = res.tx_timestamp
print(ntp_time)
