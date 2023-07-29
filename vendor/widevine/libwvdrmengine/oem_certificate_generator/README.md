OEM certificate generation tool
===================================================

## Supports

  - Generating CSR (certificate signing request)
  - Generating OEM intermediate certificate (for testing)
  - Generating OEM leaf certificate chain
  - Erasing file securely
  - Getting CSR/certificate/certificate chain information

## Prerequirements

  - Install pip: https://pip.pypa.io/en/stable/installing/
  - Install python cryptography: https://cryptography.io/en/latest/installation/

## Usage

Run `python oem_certificate.py --help` to see available commands.

The arguments can be partially or fully loaded from a configuration file, for
example, if file "location.cfg" is,

```
  -C=US
  -ST=CA
  -L=Kirkland
  -O=Some Company
  -OU=Some Unit
```

A command of

```bash
  python oem_certificate.py generate_csr @location.cfg -CN TestDevice1       \
    --output_csr_file=csr.pem --output_private_key_file=key.der
```

is equivalent to

```bash
  python oem_certificate.py generate_csr -CN TestDevice1 -C=US -ST=CA        \
    -L=Kirkland -O='Some Company' -OU='Some Unit' --output_csr_file=csr.pem  \
    --output_private_key_file=key.der.
```

Note that
  - The arguments in the config file must be one per line;
  - The arguments should not be quoted in the config file.

The script uses a default configuration file 'oem_certificate.cfg', which will
be loaded automatically if exists.
