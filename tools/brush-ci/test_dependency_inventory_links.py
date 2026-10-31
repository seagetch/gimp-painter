import hashlib
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import matched_provenance as m
REVIEWED_FILE_LINKS=dict(m.PUBLIC_DEPENDENCY_LINKS)
REVIEWED_DIRECTORY_LINKS=dict(m.METADATA_ONLY_DEPENDENCY_LINKS)

class InventoryLinks(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.base=Path(self.tmp.name);self.root=self.base/'root';self.root.mkdir();self.host=self.base/'host';self.host.mkdir()
  self.target=self.host/'public.conf';self.target.write_bytes(b'public configuration\n');self.relative='usr/lib/example/config';self.link=self.root/self.relative;self.link.parent.mkdir(parents=True);self.link.symlink_to(self.target)
  self.archive=('locked.deb','a'*64);self.package_hash=m.sha(self.target);self.counterpart=self.root/str(self.target).lstrip('/');self.counterpart.parent.mkdir(parents=True);self.counterpart.write_bytes(self.target.read_bytes())
  self.allow={self.relative:(str(self.target),self.archive,self.package_hash)}
  self.patcher=patch.object(m,'PUBLIC_DEPENDENCY_LINKS',self.allow);self.patcher.start();self.addCleanup(self.patcher.stop)
  self.patcher2=patch.object(m,'METADATA_ONLY_DEPENDENCY_LINKS',{});self.patcher2.start();self.addCleanup(self.patcher2.stop)
 def invoke(self):
  records={};h=m.tree_hash(self.root,dict([self.archive]),records);return h,records
 def test_exact_public_link_green(self):
  h,r=self.invoke();self.assertEqual(len(h),64);self.assertEqual(r[self.relative]['host_file_sha256'],self.package_hash)
 def test_no_package_equivalence_for_generated_ca(self):
  self.allow[self.relative]=(str(self.target),self.archive,None)
  _,r=self.invoke();self.assertIsNone(r[self.relative]['package_counterpart_sha256']);self.assertIn('no-package-counterpart',r[self.relative]['scope'])
 def test_generated_hash_changes_inventory(self):
  self.allow[self.relative]=(str(self.target),self.archive,None);old,_=self.invoke();self.target.write_bytes(b'changed');new,_=self.invoke();self.assertNotEqual(old,new)
 def test_missing_archive_proof(self):
  with self.assertRaises(m.InventoryLinkError):m.tree_hash(self.root,{}, {})
 def test_wrong_archive_hash(self):
  with self.assertRaises(m.InventoryLinkError):m.tree_hash(self.root,{'locked.deb':'b'*64},{})
 def test_literal_target_mismatch_no_read(self):
  self.link.unlink();self.link.symlink_to('/etc/shadow')
  with patch.object(m,'safe_regular_sha') as read:
   with self.assertRaises(m.InventoryLinkError):self.invoke()
   read.assert_not_called()
 def test_wrong_relative_link_no_read(self):
  self.link.rename(self.link.with_name('other'))
  with patch.object(m,'safe_regular_sha') as read:
   with self.assertRaises(m.InventoryLinkError):self.invoke()
   read.assert_not_called()
 def test_regular_replacement_rejected(self):
  self.link.unlink();self.link.write_text('not link')
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_missing_target_rejected(self):
  self.target.unlink()
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_target_symlink_rejected(self):
  real=self.host/'real';self.target.rename(real);self.target.symlink_to(real)
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_parent_symlink_rejected(self):
  real=self.base/'realhost';self.host.rename(real);self.host.symlink_to(real,target_is_directory=True)
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_directory_target_rejected(self):
  self.target.unlink();self.target.mkdir()
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_fifo_target_rejected_without_blocking(self):
  self.target.unlink();os.mkfifo(self.target)
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_counterpart_changed(self):
  self.counterpart.write_bytes(b'wrong')
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_host_not_package_equal(self):
  self.target.write_bytes(b'host changed')
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_counterpart_symlink(self):
  self.counterpart.unlink();self.counterpart.symlink_to(self.target)
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_missing_expected_link(self):
  self.link.unlink()
  with self.assertRaises(m.InventoryLinkError) as e:self.invoke()
  self.assertEqual(e.exception.diagnostic['category'],'missing-expected-archive-link')
 def test_unknown_escape_finite_relative_diagnostic(self):
  self.link.unlink();p=self.root/'unknown';p.symlink_to('/etc/shadow')
  with self.assertRaises(m.InventoryLinkError) as e:self.invoke()
  d=e.exception.diagnostic;self.assertEqual(d['dependency_relative_path'],'unknown');self.assertNotIn('/etc/shadow',json.dumps(d));self.assertNotIn(str(self.base),json.dumps(d))
 def test_dangling_unknown_escape_rejected(self):
  self.link.unlink();(self.root/'unknown').symlink_to('/not-existing/private')
  with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_metadata_only_directory_not_followed(self):
  self.link.unlink();self.allow.clear();relative='usr/lib/ssl/private';p=self.root/relative;p.parent.mkdir(parents=True);p.symlink_to('/etc/ssl/private',target_is_directory=True)
  with patch.object(m,'METADATA_ONLY_DEPENDENCY_LINKS',{relative:('/etc/ssl/private',self.archive)}),patch.object(m,'safe_regular_sha') as read:
   self.invoke();read.assert_not_called()
 def test_metadata_only_wrong_target(self):
  self.link.unlink();self.allow.clear();relative='usr/lib/ssl/private';p=self.root/relative;p.parent.mkdir(parents=True);p.symlink_to('/etc/shadow')
  with patch.object(m,'METADATA_ONLY_DEPENDENCY_LINKS',{relative:('/etc/ssl/private',self.archive)}):
   with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_diagnostic_long_or_unsafe_path_hashed_only(self):
  e=m.InventoryLinkError('private path/'+'x'*300,'escaping-inventory-link');self.assertIsNone(e.diagnostic['dependency_relative_path']);self.assertEqual(len(e.diagnostic['dependency_relative_path_sha256']),64)
 def test_public_file_size_bounded_before_read(self):
  with patch.object(m,'MAX_PUBLIC_CONFIG_BYTES',1),patch.object(m.os,'read') as read:
   with self.assertRaises(m.InventoryLinkError):self.invoke()
   read.assert_not_called()
 def test_public_file_growth_rejected(self):
  real_read=os.read;changed=False
  def growing(fd,n):
   nonlocal changed
   if not changed:
    changed=True
    with self.target.open('ab') as stream:stream.write(b'extra')
   return real_read(fd,n)
  with patch.object(m.os,'read',side_effect=growing):
   with self.assertRaises(m.InventoryLinkError):self.invoke()
 def test_rgb_exact_archive_required(self):
  self.link.unlink();self.allow.clear();self.relative='usr/share/X11/rgb.txt';self.link=self.root/self.relative;self.link.parent.mkdir(parents=True);self.link.symlink_to(self.target)
  self.allow[self.relative]=(str(self.target),m.X11_ARCHIVE,self.package_hash)
  with self.assertRaises(m.InventoryLinkError):m.tree_hash(self.root,dict([m.PYTHON_ARCHIVE,m.SSL_ARCHIVE]),{})
 def test_rgb_fixture_public_counterpart_and_unknown_sibling(self):
  self.link.unlink();self.allow.clear();relative='usr/share/X11/rgb.txt';link=self.root/relative;link.parent.mkdir(parents=True);link.symlink_to(self.target)
  self.allow[relative]=(str(self.target),m.X11_ARCHIVE,self.package_hash);records={};m.tree_hash(self.root,dict([m.X11_ARCHIVE]),records);self.assertEqual(records[relative]['host_file_sha256'],self.package_hash)
  (link.parent/'unapproved.txt').symlink_to(self.target)
  with self.assertRaises(m.InventoryLinkError):m.tree_hash(self.root,dict([m.X11_ARCHIVE]),{})
 def test_actual_policy_exact_four_files(self):
  # Immutable reviewed constants, including run38 verified color database.
  self.assertEqual(set(REVIEWED_FILE_LINKS),{'usr/lib/python3.13/sitecustomize.py','usr/lib/ssl/openssl.cnf','usr/lib/ssl/cert.pem','usr/share/X11/rgb.txt'})
  self.assertEqual(REVIEWED_FILE_LINKS['usr/share/X11/rgb.txt'],('/etc/X11/rgb.txt',('x11-common_1%3a7.7+24+deb13u1_all.deb','57a981938506b26dc552f19cafeaa8c04e9b59dc7508509151f8507a9d4e5f24'),'2c8ab5acc9eb072f4cc88696834188100d05e50af5d1425501d993700aaa3164'))
  self.assertEqual(set(REVIEWED_DIRECTORY_LINKS),{'usr/lib/ssl/private','usr/lib/ssl/certs'})
  self.assertEqual(REVIEWED_FILE_LINKS['usr/lib/python3.13/sitecustomize.py'][::2],('/etc/python3.13/sitecustomize.py','43d81125d92376b1a69d53a71126a041cc9a18d8080e92dea0a2ae23be138b1e'))
  self.assertEqual(REVIEWED_FILE_LINKS['usr/lib/ssl/openssl.cnf'][::2],('/etc/ssl/openssl.cnf','a65a2cb9f4ee8ffdc7ef4f0ac600c0bdafb95b7b1ab457188ac610a62f5ad6b3'))
  self.assertEqual(REVIEWED_FILE_LINKS['usr/lib/ssl/cert.pem'][::2],('/etc/ssl/certs/ca-certificates.crt',None))
  self.assertEqual(m.PYTHON_ARCHIVE[1],'a4aa6a9a8c77f87bfe545e1d14ec42c001d189ace90e3aa75deec882b39dee49');self.assertEqual(m.SSL_ARCHIVE[1],'4ff006f431d8c2e69fa736885a3b6e779f2448535f69dbde821d9f0ff87ad8ea')

if __name__=='__main__':unittest.main()
