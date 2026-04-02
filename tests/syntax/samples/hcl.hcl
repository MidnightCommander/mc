# Generic HCL sample: demonstrate all syntax features

# Simple block
service "web" {
  port    = 8080
  address = "0.0.0.0"
}

# Nested blocks
job "example" {
  group "cache" {
    task "redis" {
      driver = "docker"

      config {
        image = "redis:7"
        ports = ["redis"]
      }

      resources {
        cpu    = 500
        memory = 256
      }
    }
  }
}

# Variables and expressions
locals {
  environment = "production"
  region      = "us-east-1"
  enabled     = true
  count       = 3
  ratio       = 0.75
  nothing     = null
}

# Conditional expression
output "message" {
  value = local.enabled ? "yes" : "no"
}

# For expression
output "names" {
  value = [for item in local.items : item.name]
}

output "filtered" {
  value = {for k, v in local.map : k => v if v != ""}
}

# String interpolation
output "greeting" {
  value = "Hello, ${local.environment}!"
}

# Heredoc
output "script" {
  value = <<-EOT
    #!/bin/bash
    echo "Hello"
    echo "World"
  EOT
}

# Operators
locals {
  sum        = 1 + 2
  diff       = 10 - 3
  product    = 4 * 5
  quotient   = 10 / 3
  remainder  = 10 % 3
  negative   = -1
  not_true   = !true
  and_result = true && false
  or_result  = true || false
  eq         = 1 == 1
  neq        = 1 != 2
  gt         = 3 > 2
  gte        = 3 >= 3
  lt         = 1 < 2
  lte        = 1 <= 1
}

# Type references
variable "config" {
  type = object({
    name    = string
    count   = number
    enabled = bool
    tags    = map(string)
    ports   = list(number)
    addrs   = set(string)
    data    = tuple([string, number])
    extra   = any
  })
}

# Splat and index expressions
output "all_ids" {
  value = resource.example[*].id
}

output "first" {
  value = resource.example[0].id
}

# Collection values
locals {
  list_val  = [1, 2, 3]
  map_val   = {key1 = "val1", key2 = "val2"}
  empty_map = {}
}

# Block with multiple labels
provisioner "remote-exec" "setup" {
  command = "echo hello"
}

# Comments
# Line comment

// Another line comment

/* Block comment */

/*
  Multi-line
  block comment
*/
