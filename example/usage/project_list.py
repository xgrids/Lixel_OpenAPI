#!/usr/bin/env python3
"""
Example: project list operations (project_list)

- Request topic : lixel/application/request/project_list  (ProjectListRequest)
- Response topic : lixel/application/response/project_list (ProjectListResponse)

action supports:
- AC_LIST_ALL_PROJECTS     list all projects
- AC_MODIFY_PROJECT_LABELS modify project labels (requires id + tag_name)
- AC_DELETE_PROJECTS       delete projects (requires id_list)

Demonstrates listing all projects.
"""
from _common import build_arg_parser, dump, make_client


def main() -> int:
    parser = build_arg_parser("project list operations")
    parser.add_argument("--action", default="AC_LIST_ALL_PROJECTS",
                        help="AC_LIST_ALL_PROJECTS/AC_MODIFY_PROJECT_LABELS/"
                             "AC_DELETE_PROJECTS")
    args = parser.parse_args()

    payload = {"action": args.action}
    # Modify label example: payload.update({"id": "xxx", "tag_name": "completed"})
    # Delete project example: payload.update({"id_list": ["id1", "id2"]})

    with make_client(args) as client:
        rsp = client.request(
            req_topic="lixel/application/request/project_list",
            req_message="ProjectListRequest",
            payload=payload,
            rsp_topic="lixel/application/response/project_list",
            rsp_message="ProjectListResponse",
        )
        dump("ProjectListResponse", rsp)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
