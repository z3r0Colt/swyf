/* Promise-based bridge into private on-device JNI inference, never a remote endpoint. */
(function(){
  let serial=0;
  const pending=new Map();
  let last={state:'extracting',detail:''};
  function status(){
    if(!window.DialtoneNative) return {state:'error',detail:'This copy is not running in the Android native AI app.'};
    try{last=JSON.parse(window.DialtoneNative.status())}catch{}
    return last;
  }
  function generate(prompt){
    return new Promise((resolve,reject)=>{
      if(!window.DialtoneNative)return reject(new Error('Native AI runtime missing'));
      if(status().state!=='ready')return reject(new Error('Bundled model is '+status().state));
      const id='call-'+(++serial);
      pending.set(id,{resolve,reject});
      try{window.DialtoneNative.generate(JSON.stringify({prompt}),id)}catch(e){pending.delete(id);reject(e)}
    });
  }
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
