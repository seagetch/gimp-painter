# Current Gaussian ancestry/subscription contract

Original15.004/15.009 supporting proof; no task prerequisite or criterion changed.
No provider implementation change was required. This tests the existing committed
Filter topology logic, not a candidate coreviewable/renderer lifetime change.

Two new selected cases avoid filter_new(), whose historical default is Edge:

1. Seven root→A→B→root→B→A→root stages. At root both groups are above the Filter;
   only the ordinary red layer is lower input. Within A/B, the independent lower
   sources are green/blue. Fourteen exact derived pixels establish these input
   choices before/after active updates. Twenty-four actual stack reorder-handler
   cardinalities prove only the current stack retains the Filter subscription,
   including final close. Fourteen retired/upper-source update signals leave
   generation/run count unchanged; seven current-source edits rerun exactly once.
2. Synchronous topology invalidation invokes a callback which reparents the
   Filter while the old refresh is still on the stack. Four real handler counts
   show the obsolete refresh does not append stale handlers. Two independent
   green/yellow output pixels verify current input; obsolete root input changes
   do not rerun; the new current source reruns exactly once.

The native invocation also reruns the committed four-scenario flip contract as
one regression case. These are selected model cases, not the entire Filter
suite, GUI evidence, old runtime comparison or a wholly green aggregate.

## Exact original shared-source duties

All six original hunks remain represented unchanged in
`migration/contracts/section15-source-duty-map.json`:
01.002/000430(enum),000431(default-handler declaration),000432(signal/default
registration),000433(set_parent emission),000434(empty default handler),
000435(class callback slot). Pinned afa43fae FilterLayer constructor actively
connects parent-changed; old Clone connection is commented and does not discharge
these Filter duties. Current committed native ancestry-changed signal and
class route supply the registration/emission counterpart. FilterImpl::attach
connects owner_ancestry, which invokes structure_changed/refresh_connections.
The two fixtures exercise direct reparent and reentrant new-stack ownership
through that existing route. Native descendant signal propagation is not a new
legacy requirement silently added to this checkpoint.

This bounded functional proof does not execute the paused generic viewable,
preview, popup, Canvas child or Bucket lifetime diagnostics. Clone extreme
geometry/resize diagnostics remain held and were not invoked. Selected source
seals compare all eight earlier held provider paths plus the unchanged Clone
provider to the committed base. Remaining15 dependencies and separate popup,
arbitrary-procedure, geometry and GEGL ROI duties remain explicit.

## Sealed execution result

PASS normal3 selected cases (two new ancestry cases plus committed flip
regression); PASS selected ASan/UBSan2 new cases. The builder freshly instruments
20 units and rebuilds53 C++ RTTI-compatibility units without sanitizer; remaining
GIMP/dependencies are uninstrumented. Leak detection is disabled. Source report
changed_after_compile and the independent selected source-after changed_after_run
are both empty. No all-upstream sanitizer claim.

Base is exact committed a84cd0b051e7b648ec9519ede82ae9541c44ea1c plus only the
owned Filter test main/include for this target. The earlier separate portrait
test overlays remained present but are not linked into this model binary.
Source archive reconstructs every recorded input from that committed base plus
the two exact test overlays, verifying each hash; it contains no evolving main
provider candidates. Aggregate status remains separately15/17, not all green.
