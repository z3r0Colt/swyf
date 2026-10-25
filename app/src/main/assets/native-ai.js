/* Promise-based bridge into private on-device JNI inference, never a remote endpoint. */
(function(){
  let serial=0;
  const pending=new Map();
  let last={state:'extracting',detail:'',percent:0};
  function status(){
    if(!window.DialtoneNative) return {state:'error',detail:'This copy is not running in the Android native AI app.'};
    try{last=JSON.parse(window.DialtoneNative.status())}catch{}
    return last;
  }
  // onPartial(textSoFar) is called while the reply is being written.
  function generate(prompt,onPartial){
    return new Promise((resolve,reject)=>{
      if(!window.DialtoneNative)return reject(new Error('Native AI runtime missing'));
      if(status().state!=='ready')return reject(new Error('Bundled model is '+status().state));
      const id='call-'+(++serial);
      pending.set(id,{resolve,reject,onPartial});
      try{window.DialtoneNative.generate(JSON.stringify({prompt}),id)}catch(e){pending.delete(id);reject(e)}
    });
  }
  window.dialtoneNativePartial=(id,text)=>{
    const item=pending.get(id);
    if(item&&item.onPartial)try{item.onPartial(text)}catch{}
  };
  window.dialtoneNativeResponse=(id,json)=>{
    const item=pending.get(id);
    if(!item)return;
    pending.delete(id);
    try{const res=JSON.parse(json);if(res.ok)item.resolve(res.reply);else item.reject(new Error(res.error||'Inference failed'))}
    catch(e){item.reject(e)}
  };
  window.dialtoneNativeStatus=window.dialtoneNativeStatus||function(obj){last=obj};
  window.DialtoneAI={status,generate};
})();
