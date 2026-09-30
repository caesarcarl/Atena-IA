# Atena 0.5.5 - Programming Identity + Silent RAG Policy

- Adds optional `identity/programming.json` domain module.
- Keeps the four base identity documents mandatory, preserving compatibility with older installs.
- Elevates silent RAG/provenance behavior into the Core policy: retrieved knowledge is used naturally and internal retrieval is not narrated unless the user asks for sources/citations/provenance.
- Explicitly forbids claiming web/file/tool/source access that did not occur.
- No database schema change.
- No new runtime dependency.
