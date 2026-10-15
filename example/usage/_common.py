"""
Common helpers for usage examples

- Add the project root to sys.path so that `example.core` can be imported (supports running from any directory)
- Provide unified command-line argument parsing (connection parameters)
- Provide a function to quickly build a client from ClientOptions
"""
import argparse
import json
import os
import sys

_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
if _ROOT not in sys.path:
    sys.path.insert(0, _ROOT)

from example.core import ClientOptions, OpenApiClient  # noqa: E402


def build_arg_parser(description: str) -> argparse.ArgumentParser:
    """Build a parser with standard connection arguments"""
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("--host", default="192.168.55.1", help="device address")
    parser.add_argument("--port", type=int, default=1883, help="MQTT port")
    parser.add_argument("--tls", action="store_true", help="use MQTTS (TLS)")
    parser.add_argument("--username", default="", help="MQTT username")
    parser.add_argument("--password", default="", help="MQTT password")
    parser.add_argument("--key", default="", help="OpenAPI Key")
    parser.add_argument("--timeout", type=float, default=10.0, help="request timeout (seconds)")
    return parser


def options_from_args(args) -> ClientOptions:
    """Build ClientOptions from command-line arguments"""
    return ClientOptions(
        host=args.host,
        port=args.port,
        use_tls=args.tls,
        username=args.username,
        password=args.password,
        openapi_key=args.key,
        request_timeout=args.timeout,
    )


def make_client(args) -> OpenApiClient:
    """Build a (not yet connected) client from command-line arguments"""
    return OpenApiClient(options_from_args(args))


def dump(title: str, data) -> None:
    """Pretty-print the result"""
    print(f"\n===== {title} =====")
    if isinstance(data, (dict, list)):
        print(json.dumps(data, ensure_ascii=False, indent=2))
    else:
        print(data)
