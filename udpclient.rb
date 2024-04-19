require 'socket.rb'

class InvalidData < Exception
end




class DspUntm
  def load(data)
    if data.size >= 4
      val = data.slice!(0, 4).unpack("N")[0]
      t = Time.at(val)
      @val = t.strftime("%d.%m.%Y %H:%M:%S")
    else
      raise InvalidData.new
    end
  end
  
  def text
    @val
  end

  def proto(str)
    r = Regexp.new("^(\\d{2})\\.(\\d{2})\\.(\\d{4}) (\\d{2})\\:(\\d{2})\\:(\\d{2})")
    m = r.match(str)
    raise InvalidData.new unless m
    str.slice!(0, m[0].size)
    untm = Time.mktime(m[3].to_i, m[2].to_i, m[1].to_i, m[4].to_i, m[5].to_i, m[6].to_i).to_i
    [untm].pack("N")
  end

end


class DspPhone
  def load(data)
    if data.size >= 12
      @val = data.slice!(0, 12)
    else
      raise InvalidData.new
    end
  end
  
  def text
    @val
  end

  def proto(str)
    raise InvalidData.new if str.size < 12
    str.slice!(0, 12)
  end
end




class DspMsgbt
  def load(data)
    if data.size >= 4
      val = data.slice!(0,4).unpack("N")[0]
      if data.size >= val
        @val = data.slice!(0, val)
      else
        raise InvalidData.new
      end
    else
      raise InvalidData.new
    end    
  end
  
  def text
    @val
  end

  def proto(str)  
    buf = [str.size].pack("N")
    buf << str
    str.replace("")
    buf
  end
end


$dsp = [DspUntm, DspPhone, DspMsgbt]



def eat_space(str)
  raise "Invalid string: #{str}" if str.size == 0 || str[0..0] != " "
  str.slice!(0, 1)
end


def create_dg(str, idx)
  sendbuf = ""
  sendbuf << [idx].pack("N")
  $dsp.each do |d|
    sendbuf << d.new.proto(str)
    eat_space(str) if d != $dsp.last
  end
  sendbuf
end


$brokeidx = 1


def load_msgs(filename)
  msgs = {}
  
  File.open(filename, "r") do |f|
    idx = 0

    r = Regexp.new("^(.*?)$")    
    while (str = f.gets) != nil
      str = str.chomp
      m = r.match(str)
      if m && !str.empty?
        msgs[idx] = create_dg(str, idx)
        # Debug log: dump protocol
        puts "msg[%03d]: #{sendbuf.unpack("C*").collect {|b| "%02x" % b} }" % idx if ARGV.include?("--dump")        
        idx = idx + 1* $brokeidx
      end
    end
  end
  msgs  
end


$sendrep = 1


def client(serv_addr, filename)
  puts "Server address: #{serv_addr}"
  s = UDPSocket.new

  ARGV.each do |argv|
    m = /^\-\-sendrep=(\d+)$/.match(argv)
    if m
      $sendrep = "#{m[1]}".to_i 
      puts "$sendrep=#{$sendrep}"
    end

    m = /^\-\-brokeidx=(\d+)$/.match(argv)
    if m
      $brokeidx = "#{m[1]}".to_i 
      puts "$brokeidx=#{$brokeidx}"
    end    
  end

  
  msgs = load_msgs(filename)
  total_cnt = msgs.size
                   
  p = serv_addr.split(":")

  
  # Send buffered datagrams to server
  loop do
    puts "#{msgs.size} msg(s) left in queue."

    # Send 10 datagrams
    cnt = 0
    msgs.each do |idx, dg|
      $sendrep.times { s.send(dg, 0, p[0], p[1]) }
      cnt = cnt + 1
      break if cnt >= 10
    end
    
    # Recv all inbound datagrams
    while ((ready = IO.select([s], nil, nil, 0.1) ) != nil)
      confirm_dg, cli = s.recvfrom_nonblock(65535)
      # Remove confirmed datagrams from queue
      while confirm_dg.size >= 4
        idx = confirm_dg.slice!(0,4).unpack("N")[0]
        msgs.delete(idx) if msgs[idx]
      end
    end

    cur_cnt = msgs.size
    break if total_cnt - cur_cnt >= 20 || cur_cnt == 0
  end

  s.close  
end

raise "Invalid args" if ARGV.size < 2
client(ARGV[0], ARGV[1])
