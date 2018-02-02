prev = Bitvisor.get_time
loop do
  Bitvisor.set_schedule
  cur = Bitvisor.get_time

  if cur - prev >= 5*1000*1000 then
    Bitvisor.print "#{cur}:tick!\n"
    prev = cur
  end
end
