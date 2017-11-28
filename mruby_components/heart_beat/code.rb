
loop do
  Bitvisor.set_schedule

  addr = Bitvisor.get_dest_macaddr.map{|i| i.to_s(16)}.join(":")
  Bitvisor.print("mac addr = #{addr}\n")
  end

end
