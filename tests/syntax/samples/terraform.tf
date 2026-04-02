# Terraform sample: demonstrate all syntax features

terraform {
  required_version = ">= 1.0"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
    azurerm = {
      source  = "hashicorp/azurerm"
      version = "~> 3.0"
    }
  }

  backend "s3" {
    bucket = "my-terraform-state"
    key    = "prod/terraform.tfstate"
    region = "us-east-1"
  }
}

# Provider configuration
provider "aws" {
  region = var.region
}

# Variables
variable "region" {
  description = "AWS region"
  type        = string
  default     = "us-east-1"
}

variable "instance_config" {
  type = object({
    ami           = string
    instance_type = string
    count         = number
    tags          = map(string)
  })
}

# Locals
locals {
  environment = "production"
  common_tags = {
    Environment = local.environment
    ManagedBy   = "terraform"
    Project     = var.project_name
  }
}

# Data source
data "aws_ami" "ubuntu" {
  most_recent = true

  filter {
    name   = "name"
    values = ["ubuntu/images/hvm-ssd/ubuntu-*"]
  }

  owners = ["099720109477"]
}

# Resource with lifecycle and provisioner
resource "aws_instance" "web" {
  count         = var.instance_config.count
  ami           = data.aws_ami.ubuntu.id
  instance_type = var.instance_config.instance_type
  subnet_id     = module.vpc.subnet_ids[count.index]

  tags = merge(local.common_tags, {
    Name = "web-${count.index}"
    Role = "webserver"
  })

  lifecycle {
    create_before_destroy = true
    prevent_destroy       = false
    ignore_changes        = [tags["UpdatedAt"]]
  }

  provisioner "remote-exec" {
    inline = [
      "sudo apt-get update",
      "sudo apt-get install -y nginx",
    ]
  }

  depends_on = [aws_security_group.web]
}

# Module call
module "vpc" {
  source = "./modules/vpc"

  cidr_block = "10.0.0.0/16"
  azs        = ["us-east-1a", "us-east-1b"]
  tags       = local.common_tags
}

# Output
output "instance_ids" {
  description = "IDs of the created instances"
  value       = aws_instance.web[*].id
}

output "first_ip" {
  value = aws_instance.web[0].public_ip
}

# Moved block
moved {
  from = aws_instance.old_web
  to   = aws_instance.web
}

# Removed block
removed {
  from = aws_instance.deprecated

  lifecycle {
    destroy = false
  }
}

# For expressions
locals {
  instance_map = {
    for idx, inst in aws_instance.web :
    inst.tags["Name"] => inst.id
  }

  filtered = [
    for inst in aws_instance.web :
    inst.id if inst.tags["Role"] == "webserver"
  ]
}

# Conditional
resource "aws_eip" "web" {
  count    = var.instance_config.count > 0 ? var.instance_config.count : 0
  instance = each.value.id
  domain   = "vpc"
}

# Dynamic block
resource "aws_security_group" "web" {
  name = "web-sg"

  dynamic "ingress" {
    for_each = var.ingress_rules
    content {
      from_port   = ingress.value.from
      to_port     = ingress.value.to
      protocol    = ingress.value.protocol
      cidr_blocks = ingress.value.cidrs
    }
  }
}

# String interpolation and heredoc
resource "aws_iam_policy" "example" {
  name = "example-${local.environment}"

  policy = <<-EOT
    {
      "Version": "2012-10-17",
      "Statement": [
        {
          "Effect": "Allow",
          "Action": "s3:GetObject",
          "Resource": "arn:aws:s3:::${var.bucket_name}/*"
        }
      ]
    }
  EOT
}

# Terraform functions in expressions
locals {
  joined    = join(",", var.list)
  upper_env = upper(local.environment)
  encoded   = base64encode("hello")
  file_data = file("${path.module}/data.json")
  tmpl      = templatefile("${path.module}/init.tpl", {name = var.name})
}
