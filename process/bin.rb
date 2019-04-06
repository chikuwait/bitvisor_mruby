module Bitvisor

    class Bindata
      def self.dumphex(byte)
        bindata =[]
        byte.times do | n |
          bindata.push(self.read(n))
        end
        bindata.map!{|s| "0x" + s.to_s(16).upcase}
      end
      def self.readhex(byte)
        "0x"+ self.read(byte).to_s(16).upcase
      end
    end
end

def helloworld(name = "no name")
    Bitvisor::Util.print "Hello,#{name}-san!\n"
end

def readEthernetFreame
    Bitvisor::Util.print"Destination mac address =#{Bitvisor::Bindata.dumphex(6).join(":")}\n"
end

$started_jpeg = false
$jpeg_bin =[]
def jpeg?
    bin = Bitvisor::Bindata.dumphex(2)
    if bin[0].eql?("0xFF") && bin[1].eql?("0xD8")
      Bitvisor::Util.print "mruby found jpeg!\n"
      $jpeg_bin =[]
      $started_jpeg = true
    end

    if $started_jpeg
      Bitvisor::Util.print "before loop\n"
      2048.times do |n|
        $jpeg_bin.push(Bitvisor::Bindata.readhex(n))
        if n > 2 && $jpeg_bin[-2].eql?("0xFF") && $jpeg_bin[-1].eql?("0xDB")
          $started_jpeg = false
          Bitvisor::Util.print "end jpeg =#{$jpeg_bin.length}\n"
          break
        end
      end
    end
end