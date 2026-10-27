// "Start with Windows": the current user's Run key (HKCU), never machine-wide.
#ifndef SA3CX_AUTOSTART_H_
#define SA3CX_AUTOSTART_H_

namespace sa3cx {
namespace autostart {

bool IsEnabled();
void SetEnabled(bool enabled);

}  // namespace autostart
}  // namespace sa3cx

#endif  // SA3CX_AUTOSTART_H_
