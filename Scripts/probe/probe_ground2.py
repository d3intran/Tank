import unreal
out.clear()
vals = [n for n in dir(unreal.TraceTypeQuery) if not n.startswith("_")]
out.append("TraceTypeQuery 成员: %s" % ", ".join(vals))
