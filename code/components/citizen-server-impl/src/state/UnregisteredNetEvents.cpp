#include "StdInc.h"

#ifdef STATE_FIVE
#include <ResourceManager.h>
#include <ResourceEventComponent.h>

namespace
{
struct PendingNetEvent
{
	std::string eventName;
	std::string eventSource;
	bool handled;
};

std::vector<PendingNetEvent> g_pendingNetEvents;

bool IsClientNetEvent(const std::string& eventName, const std::string& eventSource)
{
	return eventSource.rfind("net:", 0) == 0 && eventName.rfind("__cfx_", 0) != 0;
}
}

static InitFunction initFunction([]()
{
	fx::Resource::OnInitializeInstance.Connect([](fx::Resource* resource)
	{
		// OnTriggerEvent is reset on stop: reconnect on every start, right after the per-resource scripting filter
		resource->OnStart.Connect([resource]()
		{
			resource->GetComponent<fx::ResourceEventComponent>()->OnTriggerEvent.Connect([](const std::string& eventName, const std::string& eventPayload, const std::string& eventSource, bool* eventCanceled)
			{
				if (IsClientNetEvent(eventName, eventSource) && !g_pendingNetEvents.empty())
				{
					auto& pending = g_pendingNetEvents.back();

					if (pending.eventName == eventName && pending.eventSource == eventSource)
					{
						pending.handled = true;
					}
				}

				return true;
			},
			INT32_MIN + 1);
		});
	},
	INT32_MAX);

	fx::ResourceManager::OnInitializeInstance.Connect([](fx::ResourceManager* manager)
	{
		fwRefContainer<fx::ResourceEventManagerComponent> eventManager = manager->GetComponent<fx::ResourceEventManagerComponent>();

		eventManager->OnTriggerEvent.Connect([](const std::string& eventName, const std::string& eventPayload, const std::string& eventSource, bool* eventCanceled)
		{
			if (IsClientNetEvent(eventName, eventSource))
			{
				g_pendingNetEvents.push_back({ eventName, eventSource, false });
			}

			return true;
		},
		INT32_MAX);

		manager->OnTick.Connect([eventManager]()
		{
			if (g_pendingNetEvents.empty())
			{
				return;
			}

			auto pendingNetEvents = std::move(g_pendingNetEvents);
			g_pendingNetEvents.clear();

			for (const auto& pending : pendingNetEvents)
			{
				if (!pending.handled)
				{
					eventManager->QueueEvent2("onUnregisteredNetEvent", { "internal-" + pending.eventSource }, pending.eventName);
				}
			}
		},
		INT32_MAX);
	},
	INT32_MAX);
});
#endif
