# Copyright (c) 2024 by the Zeek Project. See LICENSE for details.

# @TEST-REQUIRES: have-spicy
# @TEST-EXEC: zeek -C -r ${TRACES}/ldap/missing_ldap_logs.pcapng %INPUT
# @TEST-EXEC: cat conn.log | zeek-cut -Cn local_orig local_resp > conn.log2 && mv conn.log2 conn.log
# @TEST-EXEC: btest-diff conn.log
# @TEST-EXEC: btest-diff ldap.log
# @TEST-EXEC: btest-diff ldap_search.log
# @TEST-EXEC: ! test -f dpd.log
#
# @TEST-DOC: Test LDAP analyzer with GSS-API integrity traffic where we can still peak into LDAP wrapped into WRAP tokens.
