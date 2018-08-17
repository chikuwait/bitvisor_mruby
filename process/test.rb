def helloworld(name = "no name")
  Bitvisor.print "Hello,#{name}-san!\n"
end

def readEthernetFreame
  macaddr = Bitvisor.readBinary.map{|i| i.to_s(16)}.join(":")
  Bitvisor.print"Destination mac address =#{macaddr}\n"
end
