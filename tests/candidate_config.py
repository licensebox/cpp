#!/usr/bin/env python3
"""No-network tests for the actual fixed-public-repository CMake pin guard."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
HELPER=Path(__file__).resolve().parents[1]/'cmake/NativeCandidate.cmake'
class Candidate(unittest.TestCase):
    def invoke(self,entry):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);(root/'contract.json').write_text(json.dumps({'clients':{'c':entry}}))
            (root/'probe.cmake').write_text(f'include("{HELPER}")\nmessage("candidate=${{LICENSEBOX_C_CANDIDATE}};ref=${{LICENSEBOX_C_VERSION}}")\n')
            env=dict(os.environ,LICENSEBOX_PROTOCOL_DIR=directory)
            return subprocess.run(['cmake','-P',str(root/'probe.cmake')],env=env,text=True,capture_output=True)
    def test_stable_fallback(self):
        p=self.invoke({'repository':'licensebox/c','release':'v0.3.0'});self.assertEqual(p.returncode,0);self.assertIn('candidate=FALSE',p.stderr)
    def test_full_sha_candidate(self):
        p=self.invoke({'repository':'licensebox/c','candidate_ref':'a'*40});self.assertEqual(p.returncode,0);self.assertIn('candidate=TRUE;ref='+'a'*40,p.stderr)
    def test_invalid_repo_ref_or_type_never_selected(self):
        for ref in [None,True,'main','v0.3.0','a'*39,'a'*41,'A'*40,'https://example.invalid/repo',1234567890123456789012345678901234567890]:
            with self.subTest(ref=ref):
                p=self.invoke({'repository':'licensebox/c','candidate_ref':ref});self.assertNotEqual(p.returncode,0);self.assertIn('Invalid fixed-repository C candidate pin',p.stderr)
        p=self.invoke({'repository':'other/c','candidate_ref':'a'*40});self.assertNotEqual(p.returncode,0);self.assertIn('Invalid fixed-repository C candidate pin',p.stderr)
if __name__=='__main__':unittest.main()
