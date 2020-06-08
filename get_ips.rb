#!/usr/bin/ruby

require 'set'


$ip_pattern = /([[:digit:]]+)\.([[:digit:]]+)\.([[:digit:]])+\.([[:digit:]]+)[[:space:]]+(nid[[:digit:]]+)/
nodes = []

IO.foreach(ENV["PBS_NODEFILE"]) do |line|
  nodes << "nid#{line.rjust(6, '0')}".chomp
end


IO.foreach("/etc/hosts") do |line|
  if match = $ip_pattern.match(line)
    if nodes.any? {|node| node.eql? match[5]}
      puts "#{match[1]}.#{match[2]}.#{match[3]}.#{match[4]}"
    end
  end
end
