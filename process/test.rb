class Bitvisor
  include Bitvisor_core
  def initialize
  end


  def self.setBinary(bin)
    @@bin = bin
  end
  def readBinary
    @@bin
  end
end

def helloworld(name = "no name")
  bitvisor = Bitvisor.new
  bitvisor.print "Hello,#{name}-san!\n"
end

def readEthernetFreame
  bitvisor = Bitvisor.new
  macaddr = bitvisor.readBinary.map{|i| i.to_s(16)}.join(":")
  bitvisor.print"Destination mac address =#{macaddr}\n"
  GC.start
end
