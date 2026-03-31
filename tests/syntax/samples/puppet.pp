# Puppet sample file exercising all TS captures

# Class definition with parameters
class mymodule::webserver (
  String $package_name = 'nginx',
  Integer $port = 80,
  Boolean $ssl_enabled = false,
  Array[String] $vhosts = [],
  Hash $options = {},
) inherits mymodule::params {

  # Package resource
  package { $package_name:
    ensure => installed,
  }

  # File resource with variable interpolation
  file { '/etc/nginx/nginx.conf':
    ensure  => file,
    owner   => 'root',
    group   => 'root',
    mode    => '0644',
    content => template('mymodule/nginx.conf.erb'),
    require => Package[$package_name],
    notify  => Service['nginx'],
  }

  # Service resource
  service { 'nginx':
    ensure    => running,
    enable    => true,
    subscribe => File['/etc/nginx/nginx.conf'],
  }

  # Conditional with if/elsif/else
  if $ssl_enabled {
    file { '/etc/nginx/ssl':
      ensure => directory,
      mode   => '0700',
    }
  } elsif $port == 443 {
    notify { 'SSL port without SSL enabled': }
  } else {
    notice('Running without SSL')
  }

  # Case statement
  case $facts['os']['family'] {
    'RedHat': {
      $config_path = '/etc/nginx/conf.d'
    }
    'Debian': {
      $config_path = '/etc/nginx/sites-enabled'
    }
    default: {
      fail("Unsupported OS: ${facts['os']['family']}")
    }
  }
}

# Defined type
define mymodule::vhost (
  String $server_name = $title,
  Integer $listen_port = 80,
  String $docroot = '/var/www/html',
  Optional[String] $ssl_cert = undef,
) {
  file { "/etc/nginx/sites-available/${server_name}":
    ensure  => file,
    content => epp('mymodule/vhost.epp', {
      'server_name' => $server_name,
      'listen_port' => $listen_port,
      'docroot'     => $docroot,
    }),
  }

  # Unless conditional
  unless $ssl_cert == undef {
    file { "/etc/ssl/${server_name}.crt":
      ensure => file,
      source => $ssl_cert,
    }
  }
}

# Node definition
node 'webserver01.example.com' {
  include mymodule::webserver

  mymodule::vhost { 'default':
    server_name => 'www.example.com',
    listen_port => 8080,
  }
}

# Node with regex
node /^db\d+\.example\.com$/ {
  class { 'mysql::server':
    root_password => 'secret',
  }
}

# Variables and expressions
$base_path = '/opt/app'
$full_path = "${base_path}/config"
$numbers = [1, 2, 3, 4, 5]
$config = {
  'key1' => 'value1',
  'key2' => 'value2',
}

# Operators
$result = (10 + 5) * 2 - 3 / 1
$match = $facts['os']['name'] =~ /Ubuntu/
$no_match = $facts['os']['name'] !~ /CentOS/
$combined = $a and $b or !$c
$compare = $x >= 10 and $y <= 20
$equal = $a == $b
$not_equal = $a != $b

# Selector expression
$package = $facts['os']['family'] ? {
  'RedHat' => 'httpd',
  'Debian' => 'apache2',
  default  => 'httpd',
}

# Resource collectors
File <| title == '/tmp/test' |>
Package <<| tag == 'webserver' |>>

# Resource chaining
Package['nginx'] -> File['/etc/nginx/nginx.conf']
  ~> Service['nginx']

# Lambda with each
$numbers.each |Integer $index, $value| {
  notice("Item ${index}: ${value}")
}

# Builtin types
Integer[1, 100] $bounded_int = 42
Float $pi = 3.14
Regexp $pattern = /^test/
Variant[String, Integer] $flexible = 'hello'

# Function declaration
function mymodule::helper(String $input) >> String {
  return "processed: ${input}"
}

# Include and require
include stdlib
require apt
