//EDT美国东部夏令时，EST美国东部标准时间
void ndp_setup() {
  TimeChangeRule EDT2 = {"EDT", json["dst_week"].as<int>(), json["dst_day"].as<int>(), json["dst_month"].as<int>(), json["dst_hour"].as<int>(), json["dst_offset"].as<int>()};
  TimeChangeRule EST2 = {"EST", json["std_week"].as<int>(), json["std_day"].as<int>(), json["std_month"].as<int>(), json["std_hour"].as<int>(), json["std_offset"].as<int>()};

  if (json["dst_enable"].as<int>() == 1) {
    TZ.setRules(EDT2, EST2);
  } else {
    TZ.setRules(EST2, EST2);
  }

  Udp.begin(localPort);//启用UDP监听以接收数据
  setSyncProvider(getNtpLocalTime);//设置同步提供者
  setSyncInterval(3600);//设置同步间隔
}
