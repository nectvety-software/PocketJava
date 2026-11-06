"""Profile signed ARM callbacks for one external J2ME game."""
import argparse
import json
import time
from pathlib import Path

from unicorn import UC_HOOK_BLOCK, UC_HOOK_CODE
from unicorn.arm_const import UC_ARM_REG_R0, UC_ARM_REG_R1

from arm_smoke import API, BASE, Machine, ROOT, finish_scan

SIGNED = ROOT / "build/PocketJava-signed.vxp"


def arm_u64(machine, symbol, arg):
    machine.call(machine.symbol(symbol), arg)
    lo = machine.u.reg_read(UC_ARM_REG_R0)
    hi = machine.u.reg_read(UC_ARM_REG_R1)
    return (hi << 32) | lo


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("jar", type=Path)
    parser.add_argument("--steps", type=int, default=140)
    parser.add_argument("--sample", type=int, default=10)
    parser.add_argument("--slow-ms", type=float, default=250.0)
    parser.add_argument("--block-profile-at", action="append", type=int, default=[])
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    output = args.output or ROOT / "build" / "arm-profile-dungsi.json"
    machine = Machine(external_jar=args.jar, vxp_path=SIGNED)
    rows = []
    api_counts = {}
    block_counts = {}
    active_step = [None]

    def count_api(_u, address, _size, _user):
        name = machine.handlers.get(address)
        if name:
            api_counts[name] = api_counts.get(name, 0) + 1

    def count_block(_u, address, _size, _user):
        step = active_step[0]
        if step is not None:
            counts = block_counts.setdefault(step, {})
            counts[address] = counts.get(address, 0) + 1

    machine.u.hook_add(UC_HOOK_CODE, count_api, begin=API, end=API + 0xffff)
    started = time.perf_counter()
    try:
        machine.call(machine.symbol("gcc_entry"), machine.resolver, 0, 0)
        machine.call(machine.callbacks["system"], 1, 0)
        finish_scan(machine)
        machine.call(machine.callbacks["key"], 2, 56)
        machine.call(machine.callbacks["key"], 2, 53)
        vm_ptr = machine.get32(machine.symbol("vm"))
        previous_instructions = arm_u64(machine, "pj_instructions", vm_ptr)

        for step in range(args.steps):
            api_counts.clear()
            active_step[0] = step if step in args.block_profile_at else None
            block_hook = None
            if active_step[0] is not None:
                block_hook = machine.u.hook_add(UC_HOOK_BLOCK, count_block, begin=BASE, end=BASE + 0xfffff)
                machine.u.ctl_flush_tb()
            machine.time += 33
            t0 = time.perf_counter()
            machine.call(machine.callbacks["timer"], 1)
            elapsed_ms = (time.perf_counter() - t0) * 1000.0
            if block_hook is not None:
                machine.u.hook_del(block_hook)
                machine.u.ctl_flush_tb()
            active_step[0] = None
            vm_ptr = machine.get32(machine.symbol("vm"))
            current = arm_u64(machine, "pj_instructions", vm_ptr) if vm_ptr else previous_instructions
            delta = current - previous_instructions
            previous_instructions = current

            error = machine.cstr(machine.symbol("status"))
            row = {
                "callback": step,
                "wall_ms": round(elapsed_ms, 3),
                "java_instructions": delta,
                "heap": machine.used,
                "heap_peak": machine.peak,
                "api_counts": dict(sorted(api_counts.items(), key=lambda item: (-item[1], item[0]))),
                "error": error,
            }
            if step in block_counts:
                top = sorted(block_counts[step].items(), key=lambda item: -item[1])[:40]
                row["top_blocks"] = [{"offset": hex(address - BASE), "hits": hits} for address, hits in top]
            rows.append(row)
            if step % args.sample == 0 or elapsed_ms >= args.slow_ms or error:
                print(json.dumps(row), flush=True)
            if error:
                break
    finally:
        try:
            machine.call(machine.callbacks["system"], 5, 0)
        except Exception:
            pass

    report = {
        "jar": str(args.jar),
        "signed_vxp": str(SIGNED),
        "callbacks": len(rows),
        "wall_seconds": round(time.perf_counter() - started, 3),
        "heap_after_quit": machine.used,
        "rows": rows,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("PROFILE", output, flush=True)


if __name__ == "__main__":
    main()
