#include <bdn/init.h>
#include <bdn/mainThread.h>


namespace bdn
{	

void CallFromMainThreadBase_::dispatch()
{
	P<ISimpleCallable> pThis = this;


	Windows::ApplicationModel::Core::CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
		Windows::UI::Core::CoreDispatcherPriority::Normal,
		ref new Windows::UI::Core::DispatchedHandler(
			[pThis]()
			{
				pThis->call();
			} ) );

}


}
