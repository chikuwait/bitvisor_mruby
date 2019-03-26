module Bitvisor

    class Bindata
      def self.dumphex(byte)
        bindata =[]
        byte.times do | n |
          bindata.push(self.read(n))
        end
        bindata.map!{|s| "0x" + s.to_s(16).upcase}
      end
    end
end
  
  def helloworld(name = "no name")
    Bitvisor::Util.print "Hello,#{name}-san!\n"
  end
  
  def readEthernetFreame
    Bitvisor::Util.print"Destination mac address =#{Bitvisor::Bindata.dumphex(6).join(":")}\n"
  end
  
  def jpeg?
    bin = Bitvisor::Bindata.dumphex(2)
    Bitvisor::Util.print "mruby found jpeg!\n" if bin[0].eql?("0xFF") && bin[1].eql?("0xD8")
  end
  