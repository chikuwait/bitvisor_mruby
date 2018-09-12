module Bitvisor
  class Bindata
    def self.dumphex(byte)
      self.read(byte).to_s(16)
    end
  end
end


def helloworld(name = "no name")
  Bitvisor::Util.print "Hello,#{name}-san!\n"
end

def readEthernetFreame
  macaddr =[]
  6.times do | n |
    macaddr.push(Bitvisor::Bindata.dumphex(n))
  end
  Bitvisor::Util.print"Destination mac address =#{macaddr.join(":")}\n"
end
