# Copyright 2017 Google LLC. All Rights Reserved.

"""OEM certificate generation tool.

Supports:
  - Generating CSR (certificate signing request)
  - Generating OEM intermediate certificate (for testing only)
  - Generating OEM leaf certificate chain
  - Erasing file securely
  - Getting CSR/certificate/certificate chain information

Prerequirements (if running the script directly):
  - Install pip: https://pip.pypa.io/en/stable/installing/
  - Install python cryptography: https://cryptography.io/en/latest/installation/

Run 'python oem_certificate.py --help' to see available commands.

The arguments can be partially or fully loaded from a configuration file, for
example, if file "location.cfg" is,

  -C=US
  -ST=CA
  -L=Kirkland
  -O=Some Company
  -OU=Some Unit

A command of
  "python oem_certificate.py generate_csr @location.cfg -CN TestDevice1 "
  "--output_csr_file=csr.pem --output_private_key_file=key.der",
is equivalent to
  "python oem_certificate.py generate_csr -CN TestDevice1 -C=US -ST=CA "
  "-L=Kirkland -O='Some Company' -OU='Some Unit' --output_csr_file=csr.pem "
  "--output_private_key_file=key.der".

Note that (1) the arguments in the config file must be one per line; (2) the
arguments should not be quoted in the config file.

The script uses a default configuration file 'oem_certificate.cfg', which will
be loaded automatically if exists.
"""

import argparse
import datetime
import os
import sys

from cryptography import utils
from cryptography import x509
from cryptography.hazmat import backends
from cryptography.hazmat.backends import openssl
from cryptography.hazmat.backends.openssl import backend
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.x509 import oid
from pyasn1.codec.der import decoder
from pyasn1.codec.der import encoder
from pyasn1.type import univ
import six


class WidevineSystemId(x509.UnrecognizedExtension):
  """Implement WidevineSystemId x509 extension."""

  # oid of Widevine system id.
  oid = oid.ObjectIdentifier('1.3.6.1.4.1.11129.4.1.1')

  def __init__(self, value):
    """Inits from raw bytes."""
    super(WidevineSystemId, self).__init__(WidevineSystemId.oid, value)

  @classmethod
  def from_int_value(cls, int_value):
    """Construct from integer system id value."""
    return cls(encoder.encode(univ.Integer(int_value)))

  def int_value(self):
    """Return the integer value of the system id."""
    return int(decoder.decode(self.value)[0])


class X509CertificateChain(object):
  """Implement x509 certificate chain object.

     cryptography does not support certificate chain (pkcs7 container) right
     now, so we have to implement it using low level functions directly.
  """

  # Disable pylint on protected-access in class X509CertificateChain.
  # pylint: disable=protected-access

  def __init__(self, certificates):
    """Inits from certificate list."""
    self._certificates = certificates

  def __iter__(self):
    """Iterates through certificates."""
    return self._certificates.__iter__()

  @classmethod
  def load_der(cls, certificate_chain_data):
    """Load from DER formatted data."""
    bio = backend._bytes_to_bio(certificate_chain_data)
    pkcs7 = backend._lib.d2i_PKCS7_bio(bio.bio, backend._ffi.NULL)
    if pkcs7 == backend._ffi.NULL:
      raise ValueError('Unable to load certificate chain')
    pkcs7 = backend._ffi.gc(pkcs7, backend._lib.PKCS7_free)
    if not backend._lib.PKCS7_type_is_signed(pkcs7):
      raise ValueError('Invalid certificate chain')

    x509_stack = pkcs7.d.sign.cert
    certificates = []
    for i in xrange(backend._lib.sk_X509_num(x509_stack)):
      x509_value = backend._ffi.gc(
          backend._lib.X509_dup(backend._lib.sk_X509_value(x509_stack, i)),
          backend._lib.X509_free)
      certificates.append(openssl.x509._Certificate(backend, x509_value))
    return cls(certificates)

  def der_bytes(self):
    """Return DER formatted bytes."""
    x509_stack = backend._ffi.gc(backend._lib.sk_X509_new_null(),
                                 backend._lib.sk_X509_free)
    for certificate in self._certificates:
      backend._lib.sk_X509_push(x509_stack, certificate._x509)

    p7 = backend._lib.PKCS7_sign(backend._ffi.NULL, backend._ffi.NULL,
                                 x509_stack, backend._ffi.NULL,
                                 backend._lib.PKCS7_DETACHED)
    p7 = backend._ffi.gc(p7, backend._lib.PKCS7_free)

    bio = backend._create_mem_bio_gc()
    backend.openssl_assert(backend._lib.i2d_PKCS7_bio(bio, p7) == 1)
    return backend._read_mem_bio(bio)


def _multiple_of_1024(key_size_str):
  """argparse custom type function for key size."""
  key_size = int(key_size_str)
  if key_size % 1024:
    msg = '%r is not multiple of 1024' % key_size_str
    raise argparse.ArgumentTypeError(msg)
  return key_size


def _valid_date(date_str):
  """argparse custom type function dates."""
  return datetime.datetime.strptime(date_str, '%Y-%m-%d')


def _get_encryption_algorithm(passphrase):
  """Convenient function to get the encryption algorithm."""
  if passphrase:
    return serialization.BestAvailableEncryption(passphrase)
  else:
    return serialization.NoEncryption()


def generate_csr(args):
  """Subparser handler for generating certificate signing request."""
  key = rsa.generate_private_key(
      public_exponent=65537,
      key_size=args.key_size,
      backend=backends.default_backend())
  args.output_private_key_file.write(
      key.private_bytes(
          encoding=serialization.Encoding.DER,
          format=serialization.PrivateFormat.PKCS8,
          encryption_algorithm=_get_encryption_algorithm(args.passphrase)))

  x509_name = [
      # Provide various details about who we are.
      x509.NameAttribute(oid.NameOID.COUNTRY_NAME,
                         six.text_type(args.country_name)),
      x509.NameAttribute(oid.NameOID.STATE_OR_PROVINCE_NAME,
                         six.text_type(args.state_or_province_name)),
      x509.NameAttribute(oid.NameOID.LOCALITY_NAME,
                         six.text_type(args.locality_name)),
      x509.NameAttribute(oid.NameOID.ORGANIZATION_NAME,
                         six.text_type(args.organization_name)),
      x509.NameAttribute(oid.NameOID.ORGANIZATIONAL_UNIT_NAME,
                         six.text_type(args.organizational_unit_name)),
  ]
  if args.common_name:
    x509_name.append(x509.NameAttribute(oid.NameOID.COMMON_NAME,
                                        six.text_type(args.common_name)))
  csr = x509.CertificateSigningRequestBuilder().subject_name(
      x509.Name(x509_name)).sign(key, hashes.SHA256(),
                                 backends.default_backend())
  args.output_csr_file.write(csr.public_bytes(serialization.Encoding.PEM))


def _random_serial_number():
  """Utility function to generate random serial number."""
  return utils.int_from_bytes(os.urandom(16), byteorder='big')


def build_certificate(subject_name, issuer_name, system_id, not_valid_before,
                      valid_duration, public_key, signing_key, ca):
  """Utility function to build certificate."""
  builder = x509.CertificateBuilder()
  builder = builder.subject_name(subject_name).issuer_name(issuer_name)

  if ca:
    builder = builder.add_extension(
        x509.SubjectKeyIdentifier.from_public_key(public_key), critical=False)
    builder = builder.add_extension(
        x509.AuthorityKeyIdentifier.from_issuer_public_key(
            signing_key.public_key()),
        critical=False)
    builder = builder.add_extension(
        x509.BasicConstraints(True, 0), critical=True)
  if system_id:
    builder = builder.add_extension(
        WidevineSystemId.from_int_value(system_id), critical=False)

  builder = builder.not_valid_before(not_valid_before).not_valid_after(
      not_valid_before + datetime.timedelta(valid_duration)).serial_number(
          _random_serial_number()).public_key(public_key)
  return builder.sign(
      private_key=signing_key,
      algorithm=hashes.SHA256(),
      backend=backends.default_backend())


def generate_intermediate_certificate(args):
  """Subparser handler for generating intermediate certificate."""
  root_cert = x509.load_der_x509_certificate(args.root_certificate_file.read(),
                                             backends.default_backend())
  root_private_key = serialization.load_der_private_key(
      args.root_private_key_file.read(),
      password=args.root_private_key_passphrase,
      backend=backends.default_backend())
  if (root_private_key.public_key().public_numbers() !=
      root_cert.public_key().public_numbers()):
    raise ValueError('Root certificate does not match with root private key')
  csr = x509.load_pem_x509_csr(args.csr_file.read(), backends.default_backend())

  certificate = build_certificate(csr.subject, root_cert.subject,
                                  args.system_id, args.not_valid_before,
                                  args.valid_duration,
                                  csr.public_key(), root_private_key, True)
  args.output_certificate_file.write(
      certificate.public_bytes(serialization.Encoding.DER))


def generate_leaf_certificate(args):
  """Subparser handler for generating leaf certificate."""
  intermediate_cert_bytes = args.intermediate_certificate_file.read()

  try:
    intermediate_cert = x509.load_pem_x509_certificate(
        intermediate_cert_bytes, backends.default_backend())
  except ValueError:
    intermediate_cert = x509.load_der_x509_certificate(
        intermediate_cert_bytes, backends.default_backend())

  intermediate_private_key = serialization.load_der_private_key(
      args.intermediate_private_key_file.read(),
      password=args.intermediate_private_key_passphrase,
      backend=backends.default_backend())
  if (intermediate_private_key.public_key().public_numbers() !=
      intermediate_cert.public_key().public_numbers()):
    raise ValueError(
        'Intermediate certificate does not match with intermediate private key')

  system_id_raw_bytes = intermediate_cert.extensions.get_extension_for_oid(
      WidevineSystemId.oid).value.value
  system_id = WidevineSystemId(system_id_raw_bytes).int_value()

  name_attributes = [
      x509.NameAttribute(oid.NameOID.COMMON_NAME, u'{0}-leaf'.format(system_id))
  ]
  name_attributes.extend([
      attribute for attribute in intermediate_cert.subject
      if attribute.oid != oid.NameOID.COMMON_NAME
  ])
  subject_name = x509.Name(name_attributes)

  leaf_private_key = rsa.generate_private_key(
      public_exponent=65537,
      key_size=args.key_size,
      backend=backends.default_backend())
  args.output_private_key_file.write(
      leaf_private_key.private_bytes(
          encoding=serialization.Encoding.DER,
          format=serialization.PrivateFormat.PKCS8,
          encryption_algorithm=_get_encryption_algorithm(args.passphrase)))

  certificate = build_certificate(subject_name, intermediate_cert.subject,
                                  system_id, args.not_valid_before,
                                  args.valid_duration,
                                  leaf_private_key.public_key(),
                                  intermediate_private_key, False)
  args.output_certificate_file.write(
      X509CertificateChain([certificate, intermediate_cert]).der_bytes())


def secure_erase(args):
  """Subparser handler for secure erasing of a file."""
  length = args.file.tell()
  for _ in xrange(args.passes):
    args.file.seek(0)
    for _ in xrange(length):
      args.file.write(os.urandom(1))
  args.file.close()
  os.remove(args.file.name)


def _certificate_as_string(cert):
  """Utility function to format certificate as string."""
  lines = ['Certificate Subject Name:']
  lines.extend(['  {0}'.format(attribute) for attribute in cert.subject])
  lines.append('Issuer Name:')
  lines.extend(['  {0}'.format(attribute) for attribute in cert.issuer])
  lines.append('Key Size: {0.key_size}'.format(cert.public_key()))
  try:
    system_id_raw_bytes = cert.extensions.get_extension_for_oid(
        WidevineSystemId.oid).value.value
    lines.append('Widevine System Id: {0}'.format(
        WidevineSystemId(system_id_raw_bytes).int_value()))
  except x509.ExtensionNotFound:
    pass
  lines.append('Not valid before: {0.not_valid_before}'.format(cert))
  lines.append('Not valid after: {0.not_valid_after}'.format(cert))
  return '\n'.join(lines)


def _csr_as_string(csr):
  """Utility function to format csr as string."""
  lines = ['CSR Subject Name:']
  lines.extend(['  {0}'.format(attribute) for attribute in csr.subject])
  lines.append('Key Size: {0.key_size}'.format(csr.public_key()))
  return '\n'.join(lines)


def _handle_csr(data):
  """Utility function for get_info to parse csr."""
  return _csr_as_string(
      x509.load_pem_x509_csr(data, backends.default_backend()))


def _handle_pem_certificate(data):
  """Utility function for get_info to parse pem certificate."""
  return _certificate_as_string(
      x509.load_pem_x509_certificate(data, backends.default_backend()))


def _handle_der_certificate(data):
  """Utility function for get_info to parse der certificate."""
  return _certificate_as_string(
      x509.load_der_x509_certificate(data, backends.default_backend()))


def _handle_certificate_chain(data):
  """Utility function for get_info to parse certificate chain."""
  return '\n\n'.join([
      _certificate_as_string(certificate)
      for certificate in X509CertificateChain.load_der(data)
  ])


def get_info(args, out=sys.stdout):
  """Subparser handler to get csr or certificate information."""
  # The input is either a CSR or a certificate, or a certificate chain.
  # Loop through the corresponding handlers one by one.
  data = args.file.read()
  for handler in [
      _handle_csr, _handle_der_certificate, _handle_pem_certificate,
      _handle_certificate_chain
  ]:
    try:
      out.write(handler(data))
      return
    except ValueError:
      pass
  print('Error occurred. The input file is not a valid CSR, nor certificate, '
        'nor certificate chain.')


def create_parser():
  """Command line parsing."""
  parser = argparse.ArgumentParser(fromfile_prefix_chars='@')
  subparsers = parser.add_subparsers()

  # Subparser for certificate signing request generation.
  parser_csr = subparsers.add_parser(
      'generate_csr', help='generate certificate signing request')
  parser_csr.add_argument(
      '--key_size',
      type=_multiple_of_1024,
      default=2048,
      help='specify RSA key size.')
  parser_csr.add_argument('-C', '--country_name', required=True)
  parser_csr.add_argument('-ST', '--state_or_province_name', required=True)
  parser_csr.add_argument('-L', '--locality_name', required=True)
  parser_csr.add_argument('-O', '--organization_name', required=True)
  parser_csr.add_argument('-OU', '--organizational_unit_name', required=True)
  parser_csr.add_argument('-CN', '--common_name', required=False)
  parser_csr.add_argument(
      '--output_csr_file', type=argparse.FileType('wb'), required=True)
  parser_csr.add_argument(
      '--output_private_key_file', type=argparse.FileType('wb'), required=True)
  parser_csr.add_argument(
      '--passphrase',
      help=('specify an optional passphrase to encrypt the private key. The '
            'private key is not encrypted if omitted.'))
  parser_csr.set_defaults(func=generate_csr)

  # Subparser for intermediate certificate generation.
  parser_intermediate_cert = subparsers.add_parser(
      'generate_intermediate_certificate',
      help=('generate intermediate certificate from csr. This should only be '
            'used for testing.'))
  parser_intermediate_cert.add_argument(
      '--not_valid_before',
      type=_valid_date,
      default=datetime.datetime.today(),
      help='certificate validity start date - format YYYY-MM-DD')
  parser_intermediate_cert.add_argument(
      '--valid_duration',
      type=int,
      default=3650,
      help='the certificate will expire after the specified number of days.')
  parser_intermediate_cert.add_argument('--system_id', type=int, required=True)
  parser_intermediate_cert.add_argument(
      '--csr_file', type=argparse.FileType('rb'), required=True)
  parser_intermediate_cert.add_argument(
      '--root_certificate_file', type=argparse.FileType('rb'), required=True)
  parser_intermediate_cert.add_argument(
      '--root_private_key_file', type=argparse.FileType('rb'), required=True)
  parser_intermediate_cert.add_argument('--root_private_key_passphrase')
  parser_intermediate_cert.add_argument(
      '--output_certificate_file', type=argparse.FileType('wb'), required=True)
  parser_intermediate_cert.set_defaults(func=generate_intermediate_certificate)

  # Subparser for leaf certificate generation.
  parser_leaf_cert = subparsers.add_parser(
      'generate_leaf_certificate', help='generate leaf certificate')
  parser_leaf_cert.add_argument(
      '--key_size',
      type=_multiple_of_1024,
      default=2048,
      help='specify RSA key size.')
  parser_leaf_cert.add_argument(
      '--not_valid_before',
      type=_valid_date,
      default=datetime.datetime.today(),
      help='certificate validity start date - format YYYY-MM-DD')
  parser_leaf_cert.add_argument(
      '--valid_duration',
      type=int,
      default=3650,
      help='the certificate will expire after the specified number of days.')
  parser_leaf_cert.add_argument(
      '--intermediate_certificate_file',
      type=argparse.FileType('rb'),
      required=True)
  parser_leaf_cert.add_argument(
      '--intermediate_private_key_file',
      type=argparse.FileType('rb'),
      required=True)
  parser_leaf_cert.add_argument('--intermediate_private_key_passphrase')
  parser_leaf_cert.add_argument(
      '--output_certificate_file', type=argparse.FileType('wb'), required=True)
  parser_leaf_cert.add_argument(
      '--output_private_key_file', type=argparse.FileType('wb'), required=True)
  parser_leaf_cert.add_argument(
      '--passphrase',
      help=('specify an optional passphrase to encrypt the private key. The '
            'private key is not encrypted if omitted.'))
  parser_leaf_cert.set_defaults(func=generate_leaf_certificate)

  # Subparser for secure file erase.
  parser_erase = subparsers.add_parser('erase', help='erase a file securely')
  parser_erase.add_argument(
      '-F', '--file', type=argparse.FileType('a'), required=True)
  parser_erase.add_argument(
      '--passes',
      type=int,
      default=3,
      help=('the file will be overwritten with random patterns for the '
            'specified number of passes'))
  parser_erase.set_defaults(func=secure_erase)

  # Subparser to get CSR or certificate or certificate chain metadata.
  parser_info = subparsers.add_parser(
      'info', help='get CSR or certificate metadata')
  parser_info.add_argument(
      '-F', '--file', type=argparse.FileType('rb'), required=True)
  parser_info.set_defaults(func=get_info)

  return parser


def main():
  args = sys.argv[1:]
  config_file_name = 'oem_certificate.cfg'
  if os.path.isfile(config_file_name):
    print 'Load from args default configuration file: ', config_file_name
    args.append('@' + config_file_name)
  parser_args = create_parser().parse_args(args)
  parser_args.func(parser_args)


if __name__ == '__main__':
  main()
