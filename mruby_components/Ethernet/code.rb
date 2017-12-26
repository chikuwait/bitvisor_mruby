prev_addr = Bitvisor.get_dest_macaddr.map{|i| i.to_s(16)}.join(":")
loop do
  Bitvisor.set_schedule
  cur_addr = Bitvisor.get_dest_macaddr.map{|i| i.to_s(16)}.join(":")
  if cur_addr != prev_addr then
    Bitvisor.print("MAC addr of dest = #{cur_addr}\n")
    prev_addr =cur_addr
  end

end
